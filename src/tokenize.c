#include "mc.h"

/* ---------- Token helpers ---------- */

bool equal(Token *tok, const char *op) {
    return strlen(op) == (size_t)tok->len &&
           memcmp(tok->loc, op, tok->len) == 0;
}

Token *skip(Token *tok, const char *op) {
    if (!equal(tok, op))
        error_at(tok->loc, "expected '%s'", op);
    return tok->next;
}

int get_number(Token *tok) {
    if (tok->kind != TK_NUM)
        error_at(tok->loc, "expected a number");
    return tok->val;
}

/* ---------- Tokenizer ---------- */

static Token *new_token(TokenKind kind, char *start, char *end) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->loc  = start;
    tok->len  = (int)(end - start);
    return tok;
}

static bool is_ident_start(int c) { return isalpha(c) || c == '_'; }
static bool is_ident_cont (int c) { return isalnum(c) || c == '_'; }

static bool is_keyword(Token *tok) {
    static const char *kws[] = {
        "return", "if", "else", "while", "for",
        "int", "char", "sizeof",
    };
    for (size_t i = 0; i < sizeof(kws)/sizeof(*kws); i++) {
        if ((int)strlen(kws[i]) == tok->len &&
            memcmp(tok->loc, kws[i], tok->len) == 0)
            return true;
    }
    return false;
}

static void convert_keywords(Token *tok) {
    for (Token *t = tok; t->kind != TK_EOF; t = t->next)
        if (t->kind == TK_IDENT && is_keyword(t))
            t->kind = TK_KEYWORD;
}

static bool starts_with(const char *p, const char *q) {
    return strncmp(p, q, strlen(q)) == 0;
}

static int read_punct(char *p) {
    static const char *two[] = {"==", "!=", "<=", ">="};
    for (size_t i = 0; i < sizeof(two)/sizeof(*two); i++)
        if (starts_with(p, two[i]))
            return 2;
    return ispunct((unsigned char)*p) ? 1 : 0;
}

/* Decode a single escape sequence at p (which points at the char after
 * the backslash). Returns the resolved byte and advances *q past the
 * sequence. Recognises: \n \t \r \0 \\ \" \' and falls back to the
 * literal char for anything else. */
static char read_escape(char *p, char **q) {
    *q = p + 1;
    switch (*p) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case '0': return '\0';
    case '\\': return '\\';
    case '"':  return '"';
    case '\'': return '\'';
    default:   return *p;
    }
}

/* Tokenize a "...". `start` points at the opening quote. Returns a
 * pointer to the byte just past the closing quote, and produces a
 * TK_STR whose ->str holds the decoded bytes and ->str_len their
 * count (NOT including a trailing '\0' — see mc.h). */
static char *read_string_literal(Token *tok, char *start) {
    /* First pass: walk to the closing quote to size the buffer. */
    char *p = start + 1;
    int   max_len = 0;
    while (*p && *p != '"') {
        if (*p == '\n') error_at(start, "unterminated string literal");
        if (*p == '\\' && p[1]) p += 2; else p += 1;
        max_len++;
    }
    if (*p != '"') error_at(start, "unterminated string literal");

    char *buf = malloc(max_len);
    int   n = 0;
    for (char *q = start + 1; q < p; ) {
        if (*q == '\\') {
            buf[n++] = read_escape(q + 1, &q);
        } else {
            buf[n++] = *q++;
        }
    }

    tok->kind    = TK_STR;
    tok->loc     = start;
    tok->len     = (int)(p + 1 - start);   /* including both quotes */
    tok->str     = buf;
    tok->str_len = n;
    return p + 1;
}

Token *tokenize(char *p) {
    user_input = p;
    Token head = {0};
    Token *cur = &head;

    while (*p) {
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }

        /* Line comment. */
        if (p[0] == '/' && p[1] == '/') {
            p += 2;
            while (*p && *p != '\n') p++;
            continue;
        }

        /* Block comment. */
        if (p[0] == '/' && p[1] == '*') {
            char *q = strstr(p + 2, "*/");
            if (!q) error_at(p, "unterminated block comment");
            p = q + 2;
            continue;
        }

        if (isdigit((unsigned char)*p)) {
            char *q = p;
            long  v = strtol(p, &p, 10);
            cur = cur->next = new_token(TK_NUM, q, p);
            cur->val = (int)v;
            continue;
        }

        if (*p == '"') {
            cur = cur->next = new_token(TK_STR, p, p);
            p = read_string_literal(cur, p);
            continue;
        }

        if (is_ident_start((unsigned char)*p)) {
            char *q = p;
            do { p++; } while (is_ident_cont((unsigned char)*p));
            cur = cur->next = new_token(TK_IDENT, q, p);
            continue;
        }

        int punct_len = read_punct(p);
        if (punct_len) {
            cur = cur->next = new_token(TK_PUNCT, p, p + punct_len);
            p += punct_len;
            continue;
        }

        error_at(p, "invalid token");
    }

    cur->next = new_token(TK_EOF, p, p);
    convert_keywords(head.next);
    return head.next;
}
