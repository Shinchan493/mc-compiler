#include "mc.h"

/*
 * Stack-machine x86-64 backend, System V ABI.
 *
 *   gen_addr(n) ── leaves the address  of n's lvalue in %rax
 *   gen_expr(n) ── leaves the value of n              in %rax
 *
 * For each binary op we eval rhs, push, eval lhs, pop rhs into rdi,
 * then op rdi,rax. This costs a memory round-trip per sub-expression
 * but it's bulletproof (no register pressure to think about).
 */

static int depth;

/* Monotonic counter for unique label IDs. */
static int count(void) {
    static int i = 1;
    return i++;
}

static void push(void) {
    printf("  push %%rax\n");
    depth++;
}

static void pop(const char *reg) {
    printf("  pop %s\n", reg);
    depth--;
}

static void gen_expr(Node *node);

static void gen_addr(Node *node) {
    if (node->kind == ND_VAR) {
        printf("  lea %d(%%rbp), %%rax\n", node->var->offset);
        return;
    }
    error("not an lvalue");
}

static void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        printf("  mov $%d, %%rax\n", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        printf("  neg %%rax\n");
        return;
    case ND_VAR:
        gen_addr(node);
        printf("  mov (%%rax), %%rax\n");
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        pop("%rdi");
        printf("  mov %%rax, (%%rdi)\n");
        return;
    default:
        break;
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("%rdi");

    switch (node->kind) {
    case ND_ADD: printf("  add %%rdi, %%rax\n"); return;
    case ND_SUB: printf("  sub %%rdi, %%rax\n"); return;
    case ND_MUL: printf("  imul %%rdi, %%rax\n"); return;
    case ND_DIV:
        printf("  cqo\n");
        printf("  idiv %%rdi\n");
        return;
    case ND_EQ: case ND_NE: case ND_LT: case ND_LE:
        printf("  cmp %%rdi, %%rax\n");
        if (node->kind == ND_EQ) printf("  sete  %%al\n");
        if (node->kind == ND_NE) printf("  setne %%al\n");
        if (node->kind == ND_LT) printf("  setl  %%al\n");
        if (node->kind == ND_LE) printf("  setle %%al\n");
        printf("  movzb %%al, %%rax\n");
        return;
    default:
        error("invalid expression node");
    }
}

static void gen_stmt(Node *node) {
    switch (node->kind) {
    case ND_RETURN:
        gen_expr(node->lhs);
        printf("  jmp .L.return\n");
        return;
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(n);
        return;
    case ND_IF: {
        int c = count();
        gen_expr(node->cond);
        printf("  cmp $0, %%rax\n");
        printf("  je  .L.else.%d\n", c);
        gen_stmt(node->then);
        printf("  jmp .L.end.%d\n", c);
        printf(".L.else.%d:\n", c);
        if (node->els) gen_stmt(node->els);
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    default:
        error("invalid statement");
    }
}

void codegen(Function *prog) {
    printf("  .text\n");
    printf("  .globl main\n");
    printf("main:\n");

    /* Prologue. */
    printf("  push %%rbp\n");
    printf("  mov %%rsp, %%rbp\n");
    if (prog->stack_size)
        printf("  sub $%d, %%rsp\n", prog->stack_size);

    for (Node *n = prog->body; n; n = n->next) {
        gen_stmt(n);
        assert(depth == 0);
    }

    /* Implicit return 0 if execution falls off the end. */
    printf("  mov $0, %%rax\n");
    printf(".L.return:\n");
    printf("  mov %%rbp, %%rsp\n");
    printf("  pop %%rbp\n");
    printf("  ret\n");
}
