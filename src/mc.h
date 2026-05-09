#ifndef MC_H
#define MC_H

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward decls. Several headers below take pointers to these. */
typedef struct Node Node;
typedef struct Type Type;

/* ===== error.c ===== */
void error(const char *fmt, ...);
void error_at(char *loc, const char *fmt, ...);
void verror_at(char *loc, const char *fmt, va_list ap);

extern char *user_input;

/* ===== tokenize.c ===== */
typedef enum {
    TK_IDENT,
    TK_PUNCT,
    TK_KEYWORD,
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

/* ===== type.c ===== */

typedef enum {
    TY_INT,
    TY_PTR,
    TY_ARRAY,
} TypeKind;

struct Type {
    TypeKind kind;
    Type    *base;       /* TY_PTR/TY_ARRAY : element type */
    int      array_len;  /* TY_ARRAY only */
};

extern Type *ty_int;

bool   is_integer(Type *ty);
Type  *pointer_to(Type *base);
Type  *array_of(Type *base, int len);
int    size_of(Type *ty);
void   add_type(Node *node);

/* ===== parse.c ===== */

typedef struct Obj Obj;
struct Obj {
    Obj  *next;
    char *name;
    Type *ty;
    int   offset;
};

typedef struct Function Function;
struct Function {
    Function *next;
    char     *name;
    int       n_params;
    struct Node *body;
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
    ND_ASSIGN,
    ND_ADDR,        /* unary & */
    ND_DEREF,       /* unary * */
    ND_VAR,
    ND_NUM,
    ND_FUNCALL,
    ND_RETURN,
    ND_BLOCK,
    ND_IF,
    ND_FOR,
    ND_EXPR_STMT,
} NodeKind;

struct Node {
    NodeKind kind;
    Node *next;
    Type *ty;       /* set by add_type after construction */
    Token *tok;     /* representative token, for diagnostics */
    Node *lhs;
    Node *rhs;
    Node *body;
    Node *init;
    Node *cond;
    Node *inc;
    Node *then;
    Node *els;
    char *funcname;
    Node *args;
    Obj  *var;
    int   val;
};

Function *parse(Token *tok);

/* ===== codegen.c ===== */
void codegen(Function *prog);

#endif /* MC_H */
