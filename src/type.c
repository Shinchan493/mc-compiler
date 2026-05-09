#include "mc.h"

/*
 * Tiny type system. For this milestone there are exactly two type
 * shapes:
 *   ty_int      — singleton 'int'
 *   pointer_to(t) — synthesises a fresh TY_PTR with ->base = t
 *
 * After parsing, codegen calls add_type() over the AST so each node
 * gets a ->ty. add_type is the only place that bakes in pointer-arith
 * semantics: 'p + n' (pointer + integer) is rewritten into
 * 'p + (n * sizeof *p)' so codegen can stay scalar-only.
 */

static Type ty_int_sentinel = { TY_INT, NULL };
Type *ty_int = &ty_int_sentinel;

bool is_integer(Type *ty) {
    return ty && ty->kind == TY_INT;
}

Type *pointer_to(Type *base) {
    Type *t = calloc(1, sizeof(Type));
    t->kind = TY_PTR;
    t->base = base;
    return t;
}

static int sizeof_ty(Type *ty) {
    if (!ty) return 8;
    if (ty->kind == TY_INT) return 8;   /* we treat 'int' as 8 bytes */
    if (ty->kind == TY_PTR) return 8;
    return 8;
}

void add_type(Node *node) {
    if (!node || node->ty) return;

    add_type(node->lhs);
    add_type(node->rhs);
    add_type(node->cond);
    add_type(node->then);
    add_type(node->els);
    add_type(node->init);
    add_type(node->inc);

    for (Node *n = node->body; n; n = n->next) add_type(n);
    for (Node *n = node->args; n; n = n->next) add_type(n);

    switch (node->kind) {
    case ND_NUM:
        node->ty = ty_int;
        return;
    case ND_VAR:
        node->ty = node->var->ty;
        return;
    case ND_FUNCALL:
        node->ty = ty_int;          /* all functions return int for now */
        return;
    case ND_NEG:
    case ND_MUL:
    case ND_DIV:
    case ND_EQ: case ND_NE: case ND_LT: case ND_LE:
        node->ty = ty_int;
        return;
    case ND_ASSIGN:
        node->ty = node->lhs->ty;
        return;
    case ND_ADDR:
        node->ty = pointer_to(node->lhs->ty);
        return;
    case ND_DEREF:
        if (node->lhs->ty && node->lhs->ty->kind == TY_PTR)
            node->ty = node->lhs->ty->base;
        else
            node->ty = ty_int;
        return;
    case ND_ADD: {
        /* num + num */
        if (is_integer(node->lhs->ty) && is_integer(node->rhs->ty)) {
            node->ty = ty_int;
            return;
        }
        /* canonicalize 'num + ptr' to 'ptr + num' */
        if (is_integer(node->lhs->ty) && node->rhs->ty && node->rhs->ty->kind == TY_PTR) {
            Node *t = node->lhs; node->lhs = node->rhs; node->rhs = t;
        }
        /* ptr + num : scale num by sizeof(*ptr) */
        if (node->lhs->ty && node->lhs->ty->kind == TY_PTR && is_integer(node->rhs->ty)) {
            int sz = sizeof_ty(node->lhs->ty->base);
            Node *scale = calloc(1, sizeof(Node));
            scale->kind = ND_MUL;
            scale->lhs  = node->rhs;
            Node *k = calloc(1, sizeof(Node));
            k->kind = ND_NUM;
            k->val  = sz;
            k->ty   = ty_int;
            scale->rhs = k;
            scale->ty  = ty_int;
            node->rhs = scale;
            node->ty  = node->lhs->ty;
            return;
        }
        error("invalid + operands");
        return;
    }
    case ND_SUB: {
        if (is_integer(node->lhs->ty) && is_integer(node->rhs->ty)) {
            node->ty = ty_int;
            return;
        }
        /* ptr - int : scale int by sizeof(*ptr) */
        if (node->lhs->ty && node->lhs->ty->kind == TY_PTR && is_integer(node->rhs->ty)) {
            int sz = sizeof_ty(node->lhs->ty->base);
            Node *scale = calloc(1, sizeof(Node));
            scale->kind = ND_MUL;
            scale->lhs  = node->rhs;
            Node *k = calloc(1, sizeof(Node));
            k->kind = ND_NUM;
            k->val  = sz;
            k->ty   = ty_int;
            scale->rhs = k;
            scale->ty  = ty_int;
            node->rhs = scale;
            node->ty  = node->lhs->ty;
            return;
        }
        /* ptr - ptr : difference in elements (we leave as ty_int; not
         * needed for any current test) */
        if (node->lhs->ty && node->lhs->ty->kind == TY_PTR &&
            node->rhs->ty && node->rhs->ty->kind == TY_PTR) {
            node->ty = ty_int;
            return;
        }
        error("invalid - operands");
        return;
    }
    default:
        return;
    }
}
