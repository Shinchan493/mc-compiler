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
    TK_STR,
    TK_EOF,
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    int   val;
    char *loc;
    int   len;

    /* TK_STR only: decoded string bytes (escapes resolved) and their
     * length. The bytes do NOT include a trailing NUL — callers that
     * need one must allocate len+1 and append it. */
    char *str;
    int   str_len;
};

Token *tokenize(char *p);
bool   equal(Token *tok, const char *op);
Token *skip(Token *tok, const char *op);
int    get_number(Token *tok);

/* ===== type.c ===== */

typedef enum {
    TY_INT,
    TY_CHAR,
    TY_PTR,
    TY_ARRAY,
} TypeKind;

struct Type {
    TypeKind kind;
    Type    *base;       /* TY_PTR/TY_ARRAY : element type */
    int      array_len;  /* TY_ARRAY only */
};

extern Type *ty_int;
extern Type *ty_char;

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

    /* Locals: stack offset from %rbp. Globals: 0 (unused). */
    int   offset;

    /* Set on globals to mark them as such; locals are the default
     * (false). String literals go through this path. */
    bool  is_local;

    /* Globals only: if non-NULL, the bytes the global is initialised
     * to (e.g. the contents of a string literal, including '\0').
     * Length = init_data_len. NULL means zero-initialised. */
    char *init_data;
    int   init_data_len;
};

extern Obj *globals;
Obj *new_string_literal(const char *bytes, int len);

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
