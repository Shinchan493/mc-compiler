// expect=42
/* Pass an array (decayed) to a callee that writes through the pointer. */
int set(int *p, int i, int v) { *(p + i) = v; return 0; }
int main() {
    int a[4];
    set(a, 2, 42);
    return a[2];
}
