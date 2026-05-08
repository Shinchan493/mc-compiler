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
    TK_IDENT,   /* identifiers */
    TK_PUNCT,
    TK_KEYWORD, /* return, if, else, while, for, ... */
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

typedef struct Obj Obj;
struct Obj {
    Obj  *next;
    char *name;
    int   offset;  /* offset from rbp */
};

typedef struct Function Function;
struct Function {
    struct Node *body;     /* compound statement (a chain of stmts) */
    Obj   *locals;
    int    stack_size;
};

typedef enum {
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_EQ,
    ND_NE,
    ND_LT,
    ND_LE,
    ND_NEG,
    ND_ASSIGN,      /* = */
    ND_VAR,         /* variable reference */
    ND_NUM,
    ND_RETURN,      /* return expr ; */
    ND_BLOCK,       /* { ... } : body holds the chain */
    ND_IF,          /* if (cond) then else els */
    ND_FOR,         /* for / while loop */
    ND_EXPR_STMT,   /* expr; */
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node *next;     /* statement chain */
    Node *lhs;
    Node *rhs;
    Node *body;     /* ND_BLOCK : statement chain inside braces */
    Node *init;     /* ND_FOR (for-loop init) */
    Node *cond;     /* ND_IF, ND_FOR */
    Node *inc;      /* ND_FOR (for-loop step) */
    Node *then;     /* ND_IF, ND_FOR body */
    Node *els;      /* ND_IF */
    Obj  *var;      /* ND_VAR */
    int   val;      /* ND_NUM */
};

Function *parse(Token *tok);

/* ===== codegen.c ===== */
void codegen(Function *prog);

#endif /* MC_H */
