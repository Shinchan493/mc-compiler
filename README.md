# mc — a mini C compiler

A small, self-contained C compiler that compiles a useful subset of C
directly to x86-64 System V assembly. Hand-written tokenizer and
recursive-descent parser, stack-machine code generator, no external
dependencies beyond a libc and `gcc`/`as` for the final assemble+link step.

## Status

Work in progress. See `docs/DEVLOG.md` once it lands and the commit log
for what is currently supported.

## Build

```
make
```

Produces a single binary `mc`. Usage:

```
mc <input.c> > out.s
gcc -static out.s -o out
./out
```

## Layout

- `src/`  — compiler sources
- `test/` — regression and differential tests
- `docs/` — design notes
