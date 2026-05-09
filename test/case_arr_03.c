// expect=8
// no-diff: mc's int is 8 bytes, real gcc's is 4 — sizeof diverges.
int main() {
    int a[3];
    return sizeof(a[0]);   /* element type is int -> 8 */
}
