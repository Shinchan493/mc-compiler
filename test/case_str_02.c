// expect=5
/* Walk a NUL-terminated string and count its length. */
int strlen_(char *s) {
    int n = 0;
    while (*(s + n) != 0) n = n + 1;
    return n;
}
int main() { return strlen_("hello"); }
