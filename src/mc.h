#ifndef MC_H
#define MC_H

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== error.c ===== */
void error(const char *fmt, ...);
void error_at(char *loc, const char *fmt, ...);
void verror_at(char *loc, const char *fmt, va_list ap);

extern char *user_input;

/* ===== tokenize.c ===== */
typedef enum {
    TK_PUNCT,
    TK_NUM,
    TK_EOF,
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    int   val;
    char *loc;
    int   len;
};

Token *tokenize(char *p);
bool   equal(Token *tok, const char *op);
Token *skip(Token *tok, const char *op);
int    get_number(Token *tok);

/* ===== parse.c ===== */
typedef enum {
    ND_ADD,    /* + */
    ND_SUB,    /* - */
    ND_MUL,    /* * */
    ND_DIV,    /* / */
    ND_NUM,    /* integer */
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    int   val;   /* ND_NUM */
};

Node *parse(Token *tok);

/* ===== codegen.c ===== */
void codegen(Node *node);

#endif /* MC_H */
