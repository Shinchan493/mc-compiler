// expect=1
int main() {
    char c;
    return sizeof(c);   /* TY_CHAR has size 1, even though int is 8 */
}
