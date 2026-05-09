// expect=21
/* Sum of 0..6 via pointer arithmetic on a decayed array. */
int main() {
    int a[7];
    int i = 0;
    while (i < 7) { a[i] = i; i = i + 1; }
    int *p;
    p = a;          /* array decays to pointer */
    int s = 0;
    int j = 0;
    while (j < 7) { s = s + *(p + j); j = j + 1; }
    return s;
}
