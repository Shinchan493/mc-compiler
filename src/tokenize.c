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

/* For now: integer literals, +, -, *, /, (, ). */
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
