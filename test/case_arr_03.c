// expect=8
int main() {
    int a[3];
    return sizeof(a[0]);   /* element type is int -> 8 */
}
