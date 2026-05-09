// expect=72
/* Plain char local; assigning an int truncates to a byte on store. */
int main() {
    char c;
    c = 72;       /* 'H' */
    return c;
}
