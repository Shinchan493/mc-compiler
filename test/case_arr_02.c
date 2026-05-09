// expect=24
// no-diff: mc's int is 8 bytes, real gcc's is 4 — sizeof diverges.
/* sizeof(int) = 8, sizeof(int[3]) = 24 in mc's 8-byte-int model. */
int main() {
    int a[3];
    return sizeof(a);
}
