// expect=5
int main() {
    int a = 1;
    int b = 2;
    int c = 5;
    /* Locals are laid out by insertion order, descending. With 8-byte
     * slots (we treat int as 8 bytes), &a is below &b is below &c on
     * the stack, so &a + 2 walks to &c. */
    int *p;
    p = &a;
    return *(p + 2);
}
