// expect=24
/* sizeof(int) = 8, sizeof(int[3]) = 24 in mc's 8-byte-int model. */
int main() {
    int a[3];
    return sizeof(a);
}
