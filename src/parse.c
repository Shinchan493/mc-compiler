#include "mc.h"

/*
 * Recursive-descent parser, layered by precedence:
 *
 *   expr       = equality
 *   equality   = relational ("==" relational | "!=" relational)*
 *   relational = add ("<" add | "<=" add | ">" add | ">=" add)*
 *   add        = mul ("+" mul | "-" mul)*
 *   mul        = unary ("*" unary | "/" unary)*
 *   unary      = ("+" | "-")? primary           -- next commit
 *   primary    = num | "(" expr ")"
 *
 * '>' / '>=' are desugared to '<' / '<=' with operands swapped, so
 * codegen only needs ND_LT and ND_LE.
 */

static Node *expr(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
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
    return equality(rest, tok);
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
        /* '>'  is "lhs > rhs"  ==  "rhs < lhs" */
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
