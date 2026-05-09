// expect=42
int main() {
    int x = 7;
    int *p;
    p = &x;
    *p = 42;
    return x;
}
