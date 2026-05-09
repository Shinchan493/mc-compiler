#include "mc.h"

/*
 * Stack-machine x86-64 backend, System V ABI.
 *
 *   gen_addr(n) ── leaves the address  of n's lvalue in %rax
 *   gen_expr(n) ── leaves the value of n              in %rax
 */

static int depth;
static const char *current_fn;

/* Load the value at the address in %rax into %rax, with width chosen
 * by the result type:
 *   TY_ARRAY : don't load (array decays to address; address stays in %rax)
 *   TY_CHAR  : 1-byte signed load (movsbq)
 *   default  : 8-byte load (we treat int / ptr as 8 bytes)
 */
static void load(Type *ty) {
    if (ty && ty->kind == TY_ARRAY) return;
    if (ty && ty->kind == TY_CHAR)
        printf("  movsbq (%%rax), %%rax\n");
    else
        printf("  mov (%%rax), %%rax\n");
}

/* Store %rax into the address in %rdi, width chosen by destination type. */
static void store(Type *ty) {
    if (ty && ty->kind == TY_CHAR)
        printf("  mov %%al, (%%rdi)\n");
    else
        printf("  mov %%rax, (%%rdi)\n");
}

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
        if (node->var->is_local)
            printf("  lea %d(%%rbp), %%rax\n", node->var->offset);
        else
            printf("  lea %s(%%rip), %%rax\n", node->var->name);
        return;
    }
    if (node->kind == ND_DEREF) {
        /* '&*p' simplifies to 'p' — generate p as a value. */
        gen_expr(node->lhs);
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
        load(node->ty);
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        pop("%rdi");
        store(node->lhs->ty);
        return;
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        load(node->ty);
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

        /* System V requires %rsp to be 16-byte aligned at the call.
         * The function prologue arrives that way, but if we are mid-
         * expression with an odd number of 8-byte values pushed,
         * %rsp is off by 8 — pad with a sub/add pair around the call.
         * %al = 0 because we never pass floating-point args. */
        if (depth % 2) {
            printf("  sub $8, %%rsp\n");
            printf("  mov $0, %%rax\n");
            printf("  call %s\n", node->funcname);
            printf("  add $8, %%rsp\n");
        } else {
            printf("  mov $0, %%rax\n");
            printf("  call %s\n", node->funcname);
        }
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

/* Emit the contents of the global symbol table to the .data section.
 * For now every global has either NULL init_data (zero-fill via .zero)
 * or non-NULL init_data emitted as raw bytes. */
static void emit_data(void) {
    if (!globals) return;
    printf("  .data\n");
    for (Obj *g = globals; g; g = g->next) {
        printf("%s:\n", g->name);
        if (g->init_data) {
            for (int i = 0; i < g->init_data_len; i++)
                printf("  .byte %d\n", (unsigned char)g->init_data[i]);
        } else {
            printf("  .zero %d\n", size_of(g->ty));
        }
    }
}

void codegen(Function *prog) {
    /* Run the type pass over each function's AST so that pointer-arith
     * scaling is materialised into the tree before codegen runs. */
    for (Function *fn = prog; fn; fn = fn->next)
        add_type(fn->body);

    emit_data();
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
