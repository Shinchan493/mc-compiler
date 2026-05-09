// expect=99
/* Pass a pointer to a callee, mutate through it. */
int set99(int *p) {
    *p = 99;
    return 0;
}
int main() {
    int x = 0;
    set99(&x);
    return x;
}
