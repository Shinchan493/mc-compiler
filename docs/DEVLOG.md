# mc — design log

Short notes on the non-obvious decisions, in roughly the order they
came up. Reading order = order of pain. Each entry is one paragraph;
the goal is to have something to point at when someone asks "why is
it like that?" rather than to be exhaustive.

---

## 1. Single-pass, hand-written, recursive-descent — no yacc

A toy compiler exists to teach you how compilers work. Generating the
parser from a grammar file hides the most interesting half of the
work. mc therefore writes the lexer and parser by hand: one C function
per grammar rule, one local mutation of the token cursor at a time. It
also makes the call stack mirror the grammar, which is the single
most useful debugging property when the compiler crashes — `gdb`'s
backtrace literally reads as the production chain.

## 2. Stack-machine codegen, no register allocator

Real compilers solve register allocation as a separate problem (graph
coloring, linear scan, …). mc punts: every expression result lands in
`%rax`, every sub-expression result is `push`'d on the CPU stack and
`pop`'d into `%rdi` for the binary op. This is wasteful — the emitted
assembly is full of redundant memory traffic — but it is also
provably correct by construction, and it lets the entire backend be
~200 lines. A peephole optimizer is a natural follow-up; a real
register allocator is a project unto itself.

## 3. Why precedence climbs through layered descent, not Pratt

Two reasonable ways to encode precedence in a recursive-descent
parser exist: a stack of mutually recursive rules (one per precedence
level — `equality → relational → add → mul → unary`) or a single
expression rule that consults a precedence table (Pratt parsing). mc
uses the layered form. It is more verbose but lines up one-to-one with
the grammar comment at the top of `parse.c`, which makes the parser
trivial to extend (adding shift operators is "insert a new function
between two existing ones") and trivial to read.

## 4. The type pass runs *between* parse and codegen

`parse.c` does no type inference. It produces a tree whose nodes have
`->ty == NULL`. `type.c::add_type()` walks the tree and fills `->ty`
in. This split keeps the parser dialect-agnostic: the parser doesn't
know what types exist, only that declarators produce them. It also
gives the type pass a single place to *rewrite* the tree — pointer
arithmetic `p + n` is rewritten in-place to `p + (n * sizeof *p)` so
that codegen can stay scalar-only and never think about pointer
scaling.

## 5. Array-to-pointer decay = "skip the load"

In real C, `int a[10]` and `int *p` are different types but compile to
the same value-on-use (the address of the first element). The cleanest
way to model this without a separate "decay" AST pass is in codegen:
when emitting an `ND_VAR` or `ND_DEREF`, if the result type is
`TY_ARRAY`, simply *don't* emit the trailing `mov (%rax), %rax`. The
address that `gen_addr` already produced is exactly the value the
program wants. This is a one-line rule that handles every "array used
as expression" site uniformly.

## 6. mc treats `int` as 8 bytes

A choice, not an oversight. With 8-byte ints, every local fits in one
stack slot and the System V ABI parameter registers (`rdi`, `rsi`, …)
hold full values without sign-extension dance. The cost: `sizeof(int)`
prints `8` instead of `4`, and a handful of tests depend on that
choice. Those tests carry `// no-diff` so the differential harness
skips them. If/when mc grows a real two-tier integer model
(`int32_t` and `int64_t`), this can flip back.

## 7. Differential testing earns its keep

Regression tests check "compiler returns expected value." Differential
tests check "compiler returns the same value as `gcc -O0` does."
The second catches a class of bugs the first cannot: misparsed
operator precedence that happens to match the expected value by
accident, ABI violations that segfault on a different code path,
operand swaps in non-commutative ops. The harness in
`test/diff_test.sh` is small (~50 lines) and runs in CI on every
push.

## 8. System V vs Microsoft x64 ABI

mc emits System V — argument registers `rdi, rsi, rdx, rcx, r8, r9`,
caller-saved stack alignment 16 before `call`. This means the emitted
assembly is **not** directly runnable on Windows (where MSVC and
MinGW use a different convention: `rcx, rdx, r8, r9`, plus shadow
space). The CI runner is Ubuntu, which is the supported execution
target. Local development on Windows builds mc with MinGW and uses it
to *inspect* assembly; running the assembly requires WSL or a
Linux box.
