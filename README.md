# mc — a mini C compiler

[![ci](https://github.com/Shinchan493/mc-compiler/actions/workflows/ci.yml/badge.svg)](https://github.com/Shinchan493/mc-compiler/actions/workflows/ci.yml)

A self-contained compiler for a useful subset of C. Reads a `.c` file,
emits **x86-64 System V assembly** to stdout, which `gcc`/`as` then
turns into a real ELF executable.

Hand-written tokenizer and recursive-descent parser, stack-machine
code generator, no external dependencies beyond a C compiler and
`make`. The whole thing is ~1,400 lines of C across six files.

## Pipeline

```
  .c source ──► [Tokenizer] ──tokens──► [Parser + sema] ──typed AST──► [Codegen] ──► .s on stdout
                tokenize.c              parse.c, type.c                 codegen.c
```

The type pass (`type.c::add_type`) runs between parse and codegen.
It fills the `->ty` field on every node and rewrites pointer
arithmetic (`p + n` → `p + n * sizeof *p`) so that codegen can stay
scalar-only.

## Language reference

| Feature                                  | Status |
| ---------------------------------------- | :----: |
| Integer literals + arithmetic (`+ - * /`) |   ✅   |
| Comparison + equality (`< <= > >= == !=`) |   ✅   |
| Unary `+`, `-`, `&`, `*`, `sizeof`        |   ✅   |
| Local variables, assignment               |   ✅   |
| Compound statements `{ … }`               |   ✅   |
| `if` / `else`                             |   ✅   |
| `while`, `for`                            |   ✅   |
| `return`                                  |   ✅   |
| Function definitions, recursion           |   ✅   |
| Up to 6 `int`-typed parameters (System V) |   ✅   |
| Pointers (`*`, `&`), pointer arithmetic   |   ✅   |
| Arrays `T a[N]`, indexing `a[i]`, decay   |   ✅   |
| `char` type                               |   ✅   |
| String literals (anonymous globals)       |   ✅   |
| User-defined globals                      |   ⏳   |
| `struct` / `union` / `typedef`            |   —    |
| `float` / `double`                        |   —    |
| Preprocessor                              |   —    |

## Build

```
make
```

Produces a single binary `mc`. Usage:

```
mc <input.c> > out.s
gcc out.s -o out
./out
```

The compiler builds anywhere with a C99-or-better C compiler (tested
with MinGW gcc 15.2 on Windows and gcc on Ubuntu). The emitted assembly
only assembles+runs on x86-64 Linux — see [docs/DEVLOG.md](docs/DEVLOG.md#8-system-v-vs-microsoft-x64-abi)
for the ABI note.

## Testing

Two harnesses, both run in CI on every push:

- **`make test`** — regression suite. Each `test/case_*.c` carries a
  `// expect=<rc>` directive. We compile with mc, link with gcc,
  execute, and compare exit codes. 47 cases.
- **`make diff_test`** — differential test. Compile each case with
  *both* mc and `gcc -O0`, run both binaries, and verify identical
  exit codes and identical stdout. Tests that legitimately diverge
  from gcc (because of mc's 8-byte-int convention) carry a
  `// no-diff` directive and are skipped.

## Project layout

```
mc-compiler/
├── .github/workflows/ci.yml   GitHub Actions: build + tests + diff tests
├── Makefile                   single-target, no external deps
├── README.md
├── docs/DEVLOG.md             design notes
├── src/
│   ├── mc.h                   shared decls, AST and Token kinds
│   ├── main.c                 driver
│   ├── tokenize.c             lexer
│   ├── parse.c                recursive-descent parser
│   ├── type.c                 the small type system + add_type pass
│   ├── codegen.c              stack-machine x86-64 backend
│   └── error.c                error_at with caret diagnostics
└── test/
    ├── test.sh                regression harness
    ├── diff_test.sh           differential harness
    └── case_*.c               test programs
```

## Design notes

See [docs/DEVLOG.md](docs/DEVLOG.md) for the longer-form decisions:
why hand-written rather than yacc, why a stack machine instead of a
register allocator, why the type pass is between parse and codegen,
how array-to-pointer decay is just "skip the load", and the trade-off
that comes from treating `int` as 8 bytes.

## References

- Rui Ueyama, **chibicc** — `https://github.com/rui314/chibicc`. The
  canonical "small C compiler in commits" project; mc's architecture
  is heavily indebted to it.
- Robert Nystrom, **Crafting Interpreters** — for the parsing chapters.
- The **System V AMD64 ABI** specification (the calling convention mc
  emits) — sections 3.2.2 and 3.2.3 are what you actually need.
