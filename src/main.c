#include "mc.h"

/* Read entire file into a NUL-terminated buffer. Portable across MinGW
 * and glibc — no getline. */
static char *read_file(const char *path) {
    FILE *fp;
    if (strcmp(path, "-") == 0) {
        fp = stdin;
    } else {
        fp = fopen(path, "rb");
        if (!fp)
            error("cannot open %s", path);
    }

    size_t cap = 4096, len = 0;
    char  *buf = malloc(cap);
    if (!buf) error("out of memory");

    for (;;) {
        if (len + 1 >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
            if (!buf) error("out of memory");
        }
        size_t n = fread(buf + len, 1, cap - len - 1, fp);
        len += n;
        if (n == 0) break;
    }

    if (fp != stdin) fclose(fp);
    /* Ensure trailing newline + NUL. */
    buf[len] = '\0';
    return buf;
}

static const char *tk_name(TokenKind k) {
    switch (k) {
    case TK_PUNCT: return "PUNCT";
    case TK_NUM:   return "NUM";
    case TK_EOF:   return "EOF";
    }
    return "?";
}

static int dump_tokens(const char *path) {
    char *src = read_file(path);
    Token *tok = tokenize(src);
    for (Token *t = tok; t; t = t->next) {
        fprintf(stderr, "%-5s '%.*s'", tk_name(t->kind), t->len, t->loc);
        if (t->kind == TK_NUM) fprintf(stderr, " val=%d", t->val);
        fprintf(stderr, "\n");
        if (t->kind == TK_EOF) break;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: mc [--dump-tokens] <file.c>\n");
        return 1;
    }

    if (strcmp(argv[1], "--dump-tokens") == 0) {
        if (argc < 3) error("--dump-tokens requires a file");
        return dump_tokens(argv[2]);
    }

    /* TODO: parse + codegen will land in the next commit. For now just
     * tokenize so we can at least exercise the lexer with --dump-tokens. */
    fprintf(stderr, "mc: parser not yet implemented\n");
    return 1;
}
