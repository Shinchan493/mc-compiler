#include "mc.h"

/*
 * Recursive-descent parser, layered by precedence:
 *
 *   expr    = mul ("+" mul | "-" mul)*
 *   mul     = unary ("*" unary | "/" unary)*
 *   unary   = ("+" | "-")? primary       -- next commit
 *   primary = num | "(" expr ")"
 *
 * For this commit we only handle add/sub/mul/div + parens. Unary and
 * comparisons land in subsequent commits. The whole input is a single
 * expression terminated by ';'.
 */

static Node *expr(Token **rest, Token *tok);
static Node *mul (Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

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

static Node *new_num(int val) {
    Node *n = new_node(ND_NUM);
    n->val = val;
    return n;
}

static Node *expr(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);
    for (;;) {
        if (equal(tok, "+")) { node = new_binary(ND_ADD, node, mul(&tok, tok->next)); continue; }
        if (equal(tok, "-")) { node = new_binary(ND_SUB, node, mul(&tok, tok->next)); continue; }
        *rest = tok;
        return node;
    }
}

static Node *mul(Token **rest, Token *tok) {
    Node *node = primary(&tok, tok);
    for (;;) {
        if (equal(tok, "*")) { node = new_binary(ND_MUL, node, primary(&tok, tok->next)); continue; }
        if (equal(tok, "/")) { node = new_binary(ND_DIV, node, primary(&tok, tok->next)); continue; }
        *rest = tok;
        return node;
    }
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
    error_at(tok->loc, "expected an expression");
    return NULL; /* unreachable */
}

Node *parse(Token *tok) {
    Node *node = expr(&tok, tok);
    /* Allow an optional trailing ';' so input like "1+2;" works cleanly. */
    if (equal(tok, ";")) tok = tok->next;
    if (tok->kind != TK_EOF)
        error_at(tok->loc, "extra token");
    return node;
}
