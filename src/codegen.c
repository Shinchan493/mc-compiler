#include "mc.h"

/*
 * Stack-machine code generator.
 *
 *   gen_expr(n)  ── leaves the value of n in %rax.
 *
 * For a binary op: evaluate rhs, push, evaluate lhs, pop rhs into %rdi,
 * then op %rdi, %rax. This avoids any register allocation logic. It's
 * obviously suboptimal — every sub-expression hits memory — but it's
 * trivial to get right and to reason about, which is the whole point of
 * a teaching compiler.
 */

static int depth;

static void push(void) {
    printf("  push %%rax\n");
    depth++;
}

static void pop(const char *reg) {
    printf("  pop %s\n", reg);
    depth--;
}

static void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        printf("  mov $%d, %%rax\n", node->val);
        return;
    default:
        break;
    }

    /* Binary op: rhs first so lhs ends up on top. */
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
    default:
        error("invalid expression node");
    }
}

void codegen(Node *node) {
    printf("  .text\n");
    printf("  .globl main\n");
    printf("main:\n");
    gen_expr(node);
    printf("  ret\n");

    assert(depth == 0);
}
