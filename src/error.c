#include "mc.h"

char *user_input;

void error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

/* Print the source line containing `loc`, then a caret pointing at the
 * offending column, then the formatted message. Mimics chibicc's
 * error_at so diagnostics show actual context. */
void verror_at(char *loc, const char *fmt, va_list ap) {
    if (!user_input) {
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
        exit(1);
    }

    /* Find start of line. */
    char *line = loc;
    while (line > user_input && line[-1] != '\n')
        line--;

    /* Find end of line. */
    char *end = loc;
    while (*end && *end != '\n')
        end++;

    /* Compute 1-based line number. */
    int line_no = 1;
    for (char *p = user_input; p < line; p++)
        if (*p == '\n')
            line_no++;

    int indent = fprintf(stderr, "%d: ", line_no);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    int pos = (int)(loc - line) + indent;
    fprintf(stderr, "%*s", pos, "");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_at(char *loc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
    va_end(ap); /* unreachable */
}
