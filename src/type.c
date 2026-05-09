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

static Type ty_int_sentinel = { TY_INT, NULL, 0 };
Type *ty_int = &ty_int_sentinel;

bool is_integer(Type *ty) {
    return ty && ty->kind == TY_INT;
}

static bool is_pointer_like(Type *ty) {
    return ty && (ty->kind == TY_PTR || ty->kind == TY_ARRAY);
}

Type *pointer_to(Type *base) {
    Type *t = calloc(1, sizeof(Type));
    t->kind = TY_PTR;
    t->base = base;
    return t;
}

Type *array_of(Type *base, int len) {
    Type *t = calloc(1, sizeof(Type));
    t->kind      = TY_ARRAY;
    t->base      = base;
    t->array_len = len;
    return t;
}

int size_of(Type *ty) {
    if (!ty)                   return 8;
    if (ty->kind == TY_INT)    return 8;   /* we treat 'int' as 8 bytes */
    if (ty->kind == TY_PTR)    return 8;
    if (ty->kind == TY_ARRAY)  return size_of(ty->base) * ty->array_len;
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
        /* &arr (where arr has type T[N]) decays to T*, not (T[N])*. */
        if (node->lhs->ty && node->lhs->ty->kind == TY_ARRAY)
            node->ty = pointer_to(node->lhs->ty->base);
        else
            node->ty = pointer_to(node->lhs->ty);
        return;
    case ND_DEREF:
        if (is_pointer_like(node->lhs->ty))
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
        /* canonicalize 'num + ptr-like' to 'ptr-like + num' */
        if (is_integer(node->lhs->ty) && is_pointer_like(node->rhs->ty)) {
            Node *t = node->lhs; node->lhs = node->rhs; node->rhs = t;
        }
        /* ptr-like + num : scale num by sizeof(element). For arrays the
         * result type is a pointer-to-element (decay). */
        if (is_pointer_like(node->lhs->ty) && is_integer(node->rhs->ty)) {
            int sz = size_of(node->lhs->ty->base);
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
            node->ty  = pointer_to(node->lhs->ty->base);
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
        /* ptr-like - int : scale int by sizeof(element) */
        if (is_pointer_like(node->lhs->ty) && is_integer(node->rhs->ty)) {
            int sz = size_of(node->lhs->ty->base);
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
            node->ty  = pointer_to(node->lhs->ty->base);
            return;
        }
        /* ptr - ptr : leave as ty_int (number of elements). */
        if (is_pointer_like(node->lhs->ty) && is_pointer_like(node->rhs->ty)) {
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
