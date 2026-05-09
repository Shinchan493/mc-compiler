// expect=8
/* &*p simplifies to p, and (p - p) is 0. Verifies double-deref works. */
int main() {
    int x = 8;
    int *p;
    p = &x;
    int **q;
    q = &p;
    return **q;
}
