#include "mc.h"

/*
 * Stack-machine x86-64 backend, System V ABI.
 *
 *   gen_addr(n) ── leaves the address  of n's lvalue in %rax
 *   gen_expr(n) ── leaves the value of n              in %rax
 */

static int depth;
static const char *current_fn;

/* Argument registers in order. */
static const char *argreg[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

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
    case ND_FUNCALL: {
        int nargs = 0;
        for (Node *a = node->args; a; a = a->next) {
            gen_expr(a);
            push();
            nargs++;
        }
        if (nargs > 6)
            error("too many arguments to %s (max 6)", node->funcname);
        /* Pop in reverse so the first arg ends up in %rdi. */
        for (int i = nargs - 1; i >= 0; i--)
            pop(argreg[i]);

        /* Per System V, %al holds the number of vector args; zero for
         * us since we don't pass floats. Also align stack to 16 bytes
         * before the call. depth counts how many 8-byte slots are
         * pushed; if odd, sub 8 to realign. */
        printf("  mov $0, %%rax\n");
        printf("  call %s\n", node->funcname);
        return;
    }
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
        printf("  jmp .L.return.%s\n", current_fn);
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
    case ND_FOR: {
        int c = count();
        if (node->init) gen_stmt(node->init);
        printf(".L.begin.%d:\n", c);
        if (node->cond) {
            gen_expr(node->cond);
            printf("  cmp $0, %%rax\n");
            printf("  je  .L.end.%d\n", c);
        }
        gen_stmt(node->then);
        if (node->inc) gen_expr(node->inc);
        printf("  jmp .L.begin.%d\n", c);
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
    for (Function *fn = prog; fn; fn = fn->next) {
        printf("  .globl %s\n", fn->name);
        printf("%s:\n", fn->name);
        current_fn = fn->name;

        /* Prologue. */
        printf("  push %%rbp\n");
        printf("  mov %%rsp, %%rbp\n");
        if (fn->stack_size)
            printf("  sub $%d, %%rsp\n", fn->stack_size);

        /* Spill incoming register args into their assigned local slots.
         * `locals` is in reverse insertion order (most-recent first), so
         * the first n_params entries are the parameters in REVERSE
         * source order. Translate the index back. */
        if (fn->n_params > 6)
            error("more than 6 parameters not supported");
        Obj *p = fn->locals;
        for (int i = 0; i < fn->n_params; i++) {
            int src_index = fn->n_params - 1 - i;
            printf("  mov %s, %d(%%rbp)\n", argreg[src_index], p->offset);
            p = p->next;
        }

        for (Node *n = fn->body; n; n = n->next) {
            gen_stmt(n);
            assert(depth == 0);
        }

        printf("  mov $0, %%rax\n");
        printf(".L.return.%s:\n", fn->name);
        printf("  mov %%rbp, %%rsp\n");
        printf("  pop %%rbp\n");
        printf("  ret\n");
    }
}
