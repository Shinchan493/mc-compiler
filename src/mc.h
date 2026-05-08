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

/* current source buffer (set by main before tokenize) */
extern char *user_input;

/* ===== tokenize.c ===== */
typedef enum {
    TK_PUNCT,   /* punctuators / operators */
    TK_NUM,     /* numeric literal */
    TK_EOF,
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    int   val;     /* TK_NUM */
    char *loc;     /* token start in source */
    int   len;     /* token length in source */
};

Token *tokenize(char *p);
bool   equal(Token *tok, const char *op);
Token *skip(Token *tok, const char *op);
int    get_number(Token *tok);

#endif /* MC_H */
