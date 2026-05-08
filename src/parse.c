#include "mc.h"

/*
 * Recursive-descent parser.
 *
 *   program   = stmt*
 *   stmt      = "return" expr ";"
 *             | expr-stmt
 *   expr-stmt = expr ";"
 *   expr      = assign
 *   assign    = equality ("=" assign)?
 *   equality  = relational ("==" relational | "!=" relational)*
 *   relational= add ("<" add | "<=" add | ">" add | ">=" add)*
 *   add       = mul ("+" mul | "-" mul)*
 *   mul       = unary ("*" unary | "/" unary)*
 *   unary     = ("+" | "-") unary | primary
 *   primary   = num | ident | "(" expr ")"
 *
 * Variables aren't declared explicitly yet — any identifier introduces
 * a new local on first use, chibicc-style. Block scoping and explicit
 * 'int x;' declarations come with the type system later.
 */

static Obj *locals;

static Node *stmt(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul (Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

/* --- AST builders --- */

static Node *new_node(NodeKind kind) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    return n;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *n = new_node(kind);
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

static Node *new_unary(NodeKind kind, Node *expr) {
    Node *n = new_node(kind);
    n->lhs = expr;
    return n;
}

static Node *new_num(int val) {
    Node *n = new_node(ND_NUM);
    n->val = val;
    return n;
}

static Node *new_var_node(Obj *var) {
    Node *n = new_node(ND_VAR);
    n->var = var;
    return n;
}

/* --- Symbol table (locals only) --- */

static Obj *find_var(Token *tok) {
    for (Obj *v = locals; v; v = v->next)
        if ((int)strlen(v->name) == tok->len &&
            memcmp(v->name, tok->loc, tok->len) == 0)
            return v;
    return NULL;
}

static Obj *new_lvar(char *name) {
    Obj *v = calloc(1, sizeof(Obj));
    v->name = name;
    v->next = locals;
    locals  = v;
    return v;
}

static char *strndup_(const char *s, int n) {
    char *p = malloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* --- Statements --- */

static Node *stmt(Token **rest, Token *tok) {
    if (equal(tok, "return")) {
        Node *node = new_unary(ND_RETURN, expr(&tok, tok->next));
        *rest = skip(tok, ";");
        return node;
    }
    return expr_stmt(rest, tok);
}

static Node *expr_stmt(Token **rest, Token *tok) {
    Node *node = new_unary(ND_EXPR_STMT, expr(&tok, tok));
    *rest = skip(tok, ";");
    return node;
}

/* --- Expressions --- */

static Node *expr(Token **rest, Token *tok) {
    return assign(rest, tok);
}

static Node *assign(Token **rest, Token *tok) {
    Node *node = equality(&tok, tok);
    if (equal(tok, "="))
        node = new_binary(ND_ASSIGN, node, assign(&tok, tok->next));
    *rest = tok;
    return node;
}

static Node *equality(Token **rest, Token *tok) {
    Node *node = relational(&tok, tok);
    for (;;) {
        if (equal(tok, "==")) { node = new_binary(ND_EQ, node, relational(&tok, tok->next)); continue; }
        if (equal(tok, "!=")) { node = new_binary(ND_NE, node, relational(&tok, tok->next)); continue; }
        *rest = tok;
        return node;
    }
}

static Node *relational(Token **rest, Token *tok) {
    Node *node = add(&tok, tok);
    for (;;) {
        if (equal(tok, "<"))  { node = new_binary(ND_LT, node, add(&tok, tok->next)); continue; }
        if (equal(tok, "<=")) { node = new_binary(ND_LE, node, add(&tok, tok->next)); continue; }
        if (equal(tok, ">"))  { node = new_binary(ND_LT, add(&tok, tok->next), node); continue; }
        if (equal(tok, ">=")) { node = new_binary(ND_LE, add(&tok, tok->next), node); continue; }
        *rest = tok;
        return node;
    }
}

static Node *add(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);
    for (;;) {
        if (equal(tok, "+")) { node = new_binary(ND_ADD, node, mul(&tok, tok->next)); continue; }
        if (equal(tok, "-")) { node = new_binary(ND_SUB, node, mul(&tok, tok->next)); continue; }
        *rest = tok;
        return node;
    }
}

static Node *mul(Token **rest, Token *tok) {
    Node *node = unary(&tok, tok);
    for (;;) {
        if (equal(tok, "*")) { node = new_binary(ND_MUL, node, unary(&tok, tok->next)); continue; }
        if (equal(tok, "/")) { node = new_binary(ND_DIV, node, unary(&tok, tok->next)); continue; }
        *rest = tok;
        return node;
    }
}

static Node *unary(Token **rest, Token *tok) {
    if (equal(tok, "+")) return unary(rest, tok->next);
    if (equal(tok, "-")) return new_unary(ND_NEG, unary(rest, tok->next));
    return primary(rest, tok);
}

static Node *primary(Token **rest, Token *tok) {
    if (equal(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }

    if (tok->kind == TK_NUM) {
        Node *node = new_num(tok->val);
        *rest = tok->next;
        return node;
    }

    if (tok->kind == TK_IDENT) {
        Obj *v = find_var(tok);
        if (!v) v = new_lvar(strndup_(tok->loc, tok->len));
        *rest = tok->next;
        return new_var_node(v);
    }

    error_at(tok->loc, "expected an expression");
    return NULL;
}

/* --- Stack frame layout --- */

static int align_to(int n, int align) {
    return (n + align - 1) / align * align;
}

static void assign_lvar_offsets(Function *prog) {
    int offset = 0;
    for (Obj *v = prog->locals; v; v = v->next) {
        offset += 8;
        v->offset = -offset; /* below rbp */
    }
    prog->stack_size = align_to(offset, 16);
}

Function *parse(Token *tok) {
    locals = NULL;

    /* Build a chain of stmt nodes via ->next. */
    Node head = {0};
    Node *cur = &head;
    while (tok->kind != TK_EOF) {
        cur = cur->next = stmt(&tok, tok);
    }

    Function *prog = calloc(1, sizeof(Function));
    prog->body   = head.next;
    prog->locals = locals;
    assign_lvar_offsets(prog);
    return prog;
}
