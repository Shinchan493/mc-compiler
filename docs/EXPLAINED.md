# mc — Explained, from scratch

A guided tour of the whole project, written for a reader who has never
seen a compiler, never written assembly, and is not sure what a
"register" is. You only need to know what a variable, a function, and
an `if` statement are, the kind of programming you might do in Class 10
in any language.

This document does three things, in order:

1. Teaches you the **background ideas** (what a compiler is, what
   assembly looks like, what a CPU register is, what the stack is, and
   so on). These are not optional — the rest of the document assumes
   them.
2. Walks through **every source file** and explains what it does,
   why each piece is structured the way it is, and what the important
   blocks of code mean line by line.
3. Traces **one tiny program** all the way from the user typing C
   source code to the operating system finally running the resulting
   binary, so you can see all the parts work together.

It is long. It is meant to be read once, slowly. After you read it
once, the codebase will look small.

---

## Table of contents

1. [The basics you need first](#1-the-basics-you-need-first)
   - 1.1 [What is a programming language?](#11-what-is-a-programming-language)
   - 1.2 [What does a compiler actually do?](#12-what-does-a-compiler-actually-do)
   - 1.3 [What does assembly look like?](#13-what-does-assembly-look-like)
   - 1.4 [What is a CPU register?](#14-what-is-a-cpu-register)
   - 1.5 [What is the stack?](#15-what-is-the-stack)
   - 1.6 [What is a calling convention (the System V ABI)?](#16-what-is-a-calling-convention-the-system-v-abi)
   - 1.7 [What is "x86-64 GAS syntax"?](#17-what-is-x86-64-gas-syntax)
   - 1.8 [The toolchain: gcc, gas, ld](#18-the-toolchain-gcc-gas-ld)
   - 1.9 [What is git, what is CI?](#19-what-is-git-what-is-ci)
2. [How mc is built (the big picture)](#2-how-mc-is-built-the-big-picture)
3. [File by file](#3-file-by-file)
   - 3.1 [`mc.h` — the shared definitions](#31-mch--the-shared-definitions)
   - 3.2 [`error.c` — friendly error messages](#32-errorc--friendly-error-messages)
   - 3.3 [`tokenize.c` — turning text into tokens](#33-tokenizec--turning-text-into-tokens)
   - 3.4 [`parse.c` — turning tokens into a tree](#34-parsec--turning-tokens-into-a-tree)
   - 3.5 [`type.c` — giving every expression a type](#35-typec--giving-every-expression-a-type)
   - 3.6 [`codegen.c` — turning the tree into assembly](#36-codegenc--turning-the-tree-into-assembly)
   - 3.7 [`main.c` — the front door](#37-mainc--the-front-door)
4. [The Makefile and the build system](#4-the-makefile-and-the-build-system)
5. [A worked example, end to end](#5-a-worked-example-end-to-end)
6. [The two real bugs we fixed](#6-the-two-real-bugs-we-fixed)
7. [Running it yourself](#7-running-it-yourself)
8. [Glossary](#8-glossary)

---

# 1. The basics you need first

## 1.1 What is a programming language?

A computer is a machine that follows instructions. A *programming
language* is a way for a human to write down those instructions in a
form that is both:

- readable by a human (so we can understand and change it later),
- and *eventually* understandable by the machine.

The catch is the word "eventually". A computer's CPU does not natively
understand C, or Python, or anything that looks like words and curly
braces. The CPU understands only **machine code** — long strings of
bits like `0100 1000 1100 0111 1100 0000 0010 1010 0000 0000 0000 0000`.
Each pattern of bits is a single instruction the hardware can carry
out, like "add these two numbers" or "store this number in memory".

There are two ways to get from "human-readable text" to "machine
code":

- An **interpreter** reads your program and immediately does what each
  line says. Python is mostly interpreted.
- A **compiler** reads your program once, translates the whole thing
  into machine code, and writes the result to a file. The next time
  you want to run your program, you just run the file directly. C
  works this way.

mc is a compiler.

## 1.2 What does a compiler actually do?

A compiler is just a translator. It takes one language as input and
produces another language as output. mc takes C as input and produces
**x86-64 assembly** as output.

Now, x86-64 assembly is *not* machine code yet. It is a human-readable
text form of machine code — every line of assembly corresponds almost
one-to-one with a single machine instruction. The reason mc emits
assembly instead of machine code directly is that turning assembly into
machine code is itself a separate, easier job, and another tool
(`as`/`gas`, see below) does that job already. Why repeat work?

So the full chain looks like this:

```
your_program.c  ──►  [ mc ]  ──►  your_program.s  ──►  [ gcc / gas ]  ──►  your_program (executable)
                              .s = "the assembly file"
```

The compiler we wrote is the box labelled `[ mc ]`.

A real compiler does many small jobs, in order. mc has four:

1. **Lexer** (also called *tokenizer*). Reads the raw text of the
   source file and chops it into small pieces called *tokens*. For
   example, `return 1+2;` is split into the tokens `return`, `1`, `+`,
   `2`, `;`. The lexer doesn't understand what these tokens *mean*; it
   just labels them.
2. **Parser**. Reads the stream of tokens and builds a tree that
   describes the *structure* of the program. The expression `1+2*3`
   becomes a tree where the root is `+`, the left child is `1`, and
   the right child is another small tree representing `2*3`. This tree
   is called an **AST** (*Abstract Syntax Tree*).
3. **Type pass**. Walks the AST and decides what *type* each piece of
   the expression has. This matters because `pointer + 1` is not the
   same as `int + 1` — the pointer one needs to be scaled by the size
   of whatever the pointer points to. The type pass is also the place
   that fixes this scaling, so the next stage doesn't have to think
   about it.
4. **Code generator** (*codegen* for short). Walks the typed AST and
   prints out one or two assembly instructions for each node. This is
   the stage that produces the `.s` file.

Every stage takes the output of the previous stage as its input. mc's
stages live in different `.c` files, one per stage. We'll look at each
one in detail in [section 3](#3-file-by-file).

## 1.3 What does assembly look like?

Here is a tiny C program:

```c
int main() {
    return 1 + 2;
}
```

And here is what mc outputs when you give it that program:

```
  .text
  .globl main
main:
  push %rbp
  mov %rsp, %rbp
  mov $2, %rax
  push %rax
  mov $1, %rax
  pop %rdi
  add %rdi, %rax
  jmp .L.return.main
  mov $0, %rax
.L.return.main:
  mov %rbp, %rsp
  pop %rbp
  ret
```

Don't try to understand every line yet. Just notice the *shape*:

- Lines that end in a colon, like `main:`, are **labels** — they're
  just names for a position in the program, like a bookmark.
- Lines that begin with a dot, like `.text` or `.globl`, are
  **directives** — they tell the assembler something *about* the
  output, but they aren't instructions themselves.
- Every other line is a single CPU instruction. Each one starts with
  a verb (`mov`, `push`, `add`, `ret`) and is followed by *operands*
  (the things the instruction acts on).

That's it. Assembly is a long list of one-word verbs with their
operands, plus some labels and directives mixed in. There are no `if`s
or `while`s — those become **jumps** (`jmp`) to labels.

The strange-looking parts:

- `%rax`, `%rbp`, `%rsp`, `%rdi` are **registers**. We'll get to them
  in a moment.
- `$2` means "the literal number two". The dollar sign means
  *immediate value*, i.e. the number itself, not "what's stored at
  address 2".
- `(%rdi)` (parentheses around a register) means *dereference* — "go
  to the memory address stored in %rdi and read or write there".

In short: a CPU instruction takes a verb, two registers/numbers, and
either reads from them, writes to them, or moves data between them.

## 1.4 What is a CPU register?

A register is a tiny piece of storage *inside* the CPU itself. It
holds one number, usually 64 bits (8 bytes). On x86-64, there are
sixteen general-purpose registers, with names like `%rax`, `%rbx`,
`%rcx`, `%rdx`, `%rsi`, `%rdi`, `%r8` through `%r15`, plus the special
ones `%rsp` and `%rbp`.

Think of registers as "the CPU's pockets". When the CPU wants to add
two numbers, it has to first put each number into a register, then run
the `add` instruction (which reads from two registers and writes the
result to one), then maybe move the result somewhere safer, like RAM.
Reading and writing from a register takes essentially zero time;
reading and writing to RAM is hundreds of times slower. That's why
real compilers go through enormous effort to keep things in registers
as long as possible (this is called **register allocation**).

mc cheats: it uses *one* register, `%rax`, for almost everything. Every
expression's result ends up in `%rax`. When mc needs to compute
something with two operands (like `a + b`), it computes one operand
into `%rax`, *pushes* that value onto the stack (more on the stack in
the next section), computes the other operand into `%rax`, then pops
the first operand back out into `%rdi`, and finally runs `add %rdi,
%rax`. This makes mc's output much longer than a real compiler's, but
it makes mc itself much simpler. This style is called a **stack
machine** code generator.

Two registers are used for special things:

- `%rsp` is the **stack pointer**. It always holds the address of the
  current top of the stack.
- `%rbp` is the **base pointer**. It always holds the address of the
  beginning of the current function's *stack frame* (we'll see what
  that is in a moment). `%rbp` doesn't move during a function; `%rsp`
  moves around as we push and pop things.

Other registers we use:

- `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9` — these are the six
  registers used to pass arguments into a function, in that order. We
  pick this set because the **System V calling convention** requires
  it (see section 1.6). When mc compiles a call like `f(1, 2, 3)`, it
  puts `1` into `%rdi`, `2` into `%rsi`, `3` into `%rdx`, and then
  runs `call f`.

## 1.5 What is the stack?

The stack is a region of memory that all programs have, and it's used
for two things:

1. To store **local variables** during a function call. When
   `main` calls `compute`, `compute` needs somewhere to put its own
   local variables. It can't reuse main's locals (main might still
   need them when compute returns). So compute carves out a fresh
   region on the stack for its own locals.
2. To **save things temporarily** during expression evaluation. When
   you compute `(a+b) * (c+d)`, you have to remember `a+b` while you
   compute `c+d`, then bring `a+b` back. The stack is where you keep
   the "remember this for a sec" values.

The stack grows *downward* in memory on x86-64. That sounds confusing,
but all it means is: when you "push" something onto the stack, the
stack pointer (`%rsp`) goes *down* by 8 bytes (on a 64-bit machine),
and the new value is written at that lower address. When you "pop",
`%rsp` goes back up by 8 bytes.

```
                          higher addresses
                         ┌──────────────┐
                         │   ...        │
                         │   main's     │
                         │   stack      │
                         │   frame      │
                         ├──────────────┤   <-- main's %rbp
   compute's frame       │   compute's  │
   begins here           │   locals     │
                         │              │
                         │              │
   "top of stack" ──►    └──────────────┘   <-- %rsp
                          lower addresses
```

When you call a function:

1. `call f` pushes the *return address* onto the stack (so f knows
   where to come back to) and jumps to `f`.
2. `f`'s **prologue** runs:
   - `push %rbp` saves the caller's base pointer.
   - `mov %rsp, %rbp` makes the new base pointer point at the current
     top of the stack — this becomes f's "anchor" for finding its
     locals.
   - `sub $N, %rsp` carves out N bytes for f's local variables. After
     this, f's locals live at `-8(%rbp)`, `-16(%rbp)`, etc.
3. f does its work.
4. f's **epilogue** runs:
   - `mov %rbp, %rsp` releases f's local storage.
   - `pop %rbp` restores the caller's base pointer.
   - `ret` pops the return address and jumps back.

`-8(%rbp)` means "8 bytes below the address stored in %rbp". So if a
function has three int locals, mc puts the first at `-8(%rbp)`, the
second at `-16(%rbp)`, and the third at `-24(%rbp)`. Reading `x` (the
first one) becomes `mov -8(%rbp), %rax`.

## 1.6 What is a calling convention (the System V ABI)?

When function `main` calls function `f`, both functions agree on:

- where to put the arguments (in which registers? in what order?),
- where the return value will appear,
- which registers `f` is allowed to clobber and which it must
  preserve,
- how the stack must be aligned at the moment of the `call`
  instruction.

This contract is called the **calling convention**, or more formally
the **ABI** (*Application Binary Interface*).

There isn't one universal ABI. There are at least two famous ones for
x86-64:

- **System V AMD64** is what Linux and macOS use. Arguments go in
  `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9` (in that order); the
  return value comes back in `%rax`; and `%rsp` must be 16-byte
  aligned at the `call`.
- **Microsoft x64** is what Windows uses. Different argument
  registers (`%rcx`, `%rdx`, `%r8`, `%r9`), and an extra "shadow
  space" reserved on the stack.

mc emits System V. That's why our binaries run on Linux but not
natively on Windows. To run mc's output on Windows, you'd need WSL
(Windows Subsystem for Linux), which is what we used during
development.

The "16-byte aligned at the `call`" rule is important. It means: at
the exact moment you execute a `call` instruction, the stack pointer
`%rsp` must be a multiple of 16. The reason is that after `call`
pushes the return address (8 bytes), the callee's `%rsp` ends with
`...8`, and the callee's `push %rbp` brings it back to a multiple of
16. Many CPU instructions (especially the SSE/SIMD ones used by glibc
internally) require 16-byte-aligned stack access — if it's off, the
program crashes.

mc had a real bug here, which we'll discuss in [section 6.1](#61-bug-1-stack-misalignment-at-mid-expression-calls).

## 1.7 What is "x86-64 GAS syntax"?

There are two popular ways of writing x86-64 assembly:

- **AT&T / GAS syntax**: source first, destination second; immediates
  prefixed with `$`; registers prefixed with `%`. Like this:
  `mov $5, %rax` means "put 5 into %rax".
- **Intel syntax**: destination first, source second; no prefixes.
  Like this: `mov rax, 5`.

mc emits AT&T / GAS syntax because that's what `gas` (the GNU
assembler, which comes with `gcc`) expects by default. If you've ever
read x86 assembly in Microsoft documentation, you've seen Intel
syntax; if you've ever read assembly in a Linux kernel patch, you've
seen AT&T. They describe the same instructions; only the spelling
differs.

## 1.8 The toolchain: gcc, gas, ld

When you type `gcc hello.c -o hello`, three programs run, in this
order:

1. The **C compiler proper** (`cc1` inside gcc). It turns `hello.c`
   into `hello.s` (assembly).
2. The **assembler** (`as`, also called `gas`). It turns `hello.s`
   into `hello.o` (an *object file* — machine code, but not yet a
   complete program).
3. The **linker** (`ld`). It takes `hello.o` plus a bunch of standard
   library object files (the C runtime startup, libc, etc.) and
   stitches them together into the final executable, `hello`.

mc replaces step 1. We still use `gcc` to do steps 2 and 3:

```
$ ./mc hello.c > hello.s        # mc replaces step 1
$ gcc hello.s -o hello          # gcc does steps 2 and 3
$ ./hello
```

Notice we hand `gcc` an `.s` file. When gcc sees an `.s` file, it
skips compilation and goes straight to assembly + linking. That's
exactly what we want.

## 1.9 What is git, what is CI?

**git** is a version control system. It records every change you
make to your code as a *commit*, with a message describing what
changed and why. The whole history is kept on your computer (and
mirrored on a remote server like GitHub). If you make a mistake, you
can roll back to any earlier commit.

A **commit** is a snapshot of the project at a point in time, plus a
short message. Good commit messages explain not just *what* changed
but *why*. mc has 18 commits, and you can read the project's history
by running `git log` in the repo.

**CI** stands for *Continuous Integration*. It is a robot that, every
time you push new code to GitHub, automatically:

1. Downloads a fresh copy of your code on a clean Linux machine.
2. Runs your build (`make`) to check the code compiles.
3. Runs your tests to check the code still works.
4. Reports back whether everything passed or something failed.

This catches bugs immediately. mc's CI runs on Ubuntu (a popular Linux
distribution) and is configured by the file `.github/workflows/ci.yml`.
The cute green "ci passing" badge at the top of the README is
generated by this CI run.

CI is how we caught the two real bugs we fixed during development —
the compiler ran fine on Windows because mc itself is just a C
program, but the *output* it produced was wrong, and we couldn't tell
without actually running the output on Linux.

---

# 2. How mc is built (the big picture)

```
  .c source ──► [Tokenizer] ──tokens──► [Parser + sema] ──typed AST──► [Codegen] ──► .s on stdout
                tokenize.c              parse.c, type.c                 codegen.c
```

That's the whole architecture. Six source files implement the boxes:

| File              | What it does                                                                          |
| ----------------- | ------------------------------------------------------------------------------------- |
| `src/mc.h`        | Shared definitions: `Token`, `Node`, `Type`, `Obj`, function prototypes for everyone. |
| `src/error.c`     | Pretty error messages with a caret pointing at the offending column.                  |
| `src/tokenize.c`  | The lexer: chops the input string into tokens.                                        |
| `src/parse.c`     | The parser: turns tokens into an AST.                                                 |
| `src/type.c`      | The type pass: fills in each AST node's type, scales pointer arithmetic.              |
| `src/codegen.c`   | The code generator: walks the typed AST and prints assembly.                          |
| `src/main.c`      | The driver: reads the input file, calls each pass in order, prints the output.        |

These six files compile to seven `.o` files (`mc.h` is just a header,
so it doesn't compile on its own — it gets *included* by the other
six). The `.o` files are linked together to make the single binary
`mc`. The `Makefile` tells `make` how to do this.

If you imagine the compiler as a small factory, then:

- `tokenize.c` is the receiving dock: it accepts the raw input.
- `parse.c` is the sorter: it lays out the input in a structured way.
- `type.c` is the inspector: it adds type-information labels to every
  piece.
- `codegen.c` is the shipping dock: it turns the typed pieces into
  machine-ready output.
- `error.c` is the loudspeaker: it tells you when something is wrong.
- `main.c` is the supervisor: it makes sure each station gets the
  output of the previous one.
- `mc.h` is the company memo: every station reads it so they all
  agree on what a "Token" is, what a "Node" is, and so on.

The total size of mc is about 1,400 lines of C across these six files.
For a compiler that handles real C programs (functions, recursion,
pointers, arrays, strings), that's tiny.

---

# 3. File by file

## 3.1 `mc.h` — the shared definitions

A **header file** in C is a file other files `#include` to get the
definitions of types and functions they use. `mc.h` is included at the
top of every other `.c` file in mc.

The first lines just include the standard library headers we'll need
everywhere:

```c
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

`stdio.h` gives us `printf`. `stdlib.h` gives us `malloc` and `exit`.
`string.h` gives us `memcpy` and `strlen`. `ctype.h` gives us
`isspace`, `isdigit`, `isalpha`. `stdbool.h` gives us the type `bool`
with values `true` and `false` (C didn't always have these — C99 added
them). `stdarg.h` gives us the machinery to write functions that take
a variable number of arguments (like `printf` itself), which we need
for our error reporter.

Then comes a *forward declaration*:

```c
typedef struct Node Node;
typedef struct Type Type;
```

`typedef struct Node Node` is C's way of saying "there is going to be
a `struct` called `Node` somewhere — believe me — and you can refer to
it by the short name `Node` from now on." We need this because `Type`
and `Node` reference each other (a `Node` has a `Type` field; a `Type`
might one day have a `Node` field), and you can't define both
completely on the same line. This is called a *forward declaration*.

Then the prototypes for `error.c`:

```c
void error(const char *fmt, ...);
void error_at(char *loc, const char *fmt, ...);
void verror_at(char *loc, const char *fmt, va_list ap);
extern char *user_input;
```

These say: "there exists a function named `error` that takes a format
string and a variable number of arguments; there's another called
`error_at` which also takes a source location; and somewhere there's
a global string `user_input`." The `...` is C's syntax for varargs.

The token kinds:

```c
typedef enum {
    TK_IDENT,    // identifiers like 'main' or 'count'
    TK_PUNCT,    // punctuation like '+' or '('
    TK_KEYWORD,  // keywords like 'return' or 'while'
    TK_NUM,      // numeric literals
    TK_STR,      // string literals "..."
    TK_EOF,      // sentinel marking the end
} TokenKind;
```

A `typedef enum { ... } TokenKind` defines a new type called
`TokenKind` that can hold any of the named values. Internally each
name is just a small integer (0, 1, 2, 3, 4, 5), but the names make
the code far more readable than `if (kind == 3)`.

The `Token` struct is the actual data each token carries:

```c
typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    int   val;
    char *loc;
    int   len;
    char *str;
    int   str_len;
};
```

- `kind` says which kind of token this is.
- `next` is a pointer to the next token in the stream. We chain tokens
  into a linked list rather than putting them in an array, because we
  don't know in advance how many tokens there will be, and a linked
  list grows naturally.
- `val` is the integer value, used only when `kind == TK_NUM`.
- `loc` is a pointer back into the source file at the position where
  this token started. We keep this so error messages can point at the
  exact column.
- `len` is how many characters this token occupies in the source.
- `str` and `str_len` are used only for `TK_STR` (string literal
  tokens) — they hold the decoded bytes of the string, with all the
  `\n`/`\t` escapes already turned into the right characters.

Then the type system:

```c
typedef enum { TY_INT, TY_CHAR, TY_PTR, TY_ARRAY } TypeKind;

struct Type {
    TypeKind kind;
    Type    *base;       // TY_PTR or TY_ARRAY: element type
    int      array_len;  // TY_ARRAY only
};
```

Four type kinds. `TY_INT` and `TY_CHAR` are *primitive*: there's
nothing inside them. `TY_PTR` (pointer) has a `base` — the type of
what it points at. `TY_ARRAY` has a `base` (the element type) and an
`array_len` (how many of them).

Two pre-built singletons:

```c
extern Type *ty_int;
extern Type *ty_char;
```

These are the global instances of "int" and "char" — every place that
needs to refer to "the int type" uses `ty_int`. We don't allocate a
fresh type every time, both for memory and for pointer-equality.

Then `Obj`, which represents a *named thing* — a variable or function:

```c
typedef struct Obj Obj;
struct Obj {
    Obj  *next;
    char *name;
    Type *ty;
    int   offset;          // locals only: offset from %rbp
    bool  is_local;        // false means global
    char *init_data;       // globals only: initial bytes
    int   init_data_len;
};
```

A linked list again. Each variable knows its name, its type, whether
it's a local or a global, and (if local) its stack offset.

Now `Function`:

```c
typedef struct Function Function;
struct Function {
    Function *next;
    char     *name;
    int       n_params;
    Node     *body;
    Obj      *locals;
    int       stack_size;
};
```

Each function in the program becomes one `Function`. `name` is its C
identifier. `n_params` is how many parameters it takes. `body` is the
AST root of its body (a single `ND_BLOCK` node containing all the
statements). `locals` is a linked list of all local variables in the
function. `stack_size` is how many bytes its stack frame needs (a
multiple of 16, after alignment).

Now the AST node kinds:

```c
typedef enum {
    ND_ADD, ND_SUB, ND_MUL, ND_DIV,           // binary arith
    ND_EQ, ND_NE, ND_LT, ND_LE,               // comparison
    ND_NEG, ND_ASSIGN, ND_ADDR, ND_DEREF,     // unary, =, &, *
    ND_VAR, ND_NUM,
    ND_FUNCALL, ND_RETURN,
    ND_BLOCK, ND_IF, ND_FOR, ND_EXPR_STMT,
} NodeKind;
```

Twenty distinct kinds of nodes. Each one is a pattern in the source
code. `ND_NUM` is for a number literal like `42`. `ND_ADD` is for any
`+` expression. `ND_IF` is for `if` statements. `ND_FUNCALL` is for
function calls. And so on.

Finally the `Node` struct itself:

```c
struct Node {
    NodeKind kind;
    Node *next;       // statement chaining
    Type *ty;         // filled in by add_type
    Token *tok;       // representative token, for diagnostics
    Node *lhs;
    Node *rhs;
    Node *body;
    Node *init;
    Node *cond;
    Node *inc;
    Node *then;
    Node *els;
    char *funcname;   // ND_FUNCALL only
    Node *args;       // ND_FUNCALL only
    Obj  *var;        // ND_VAR only
    int   val;        // ND_NUM only
};
```

This struct is *one big union of fields*. Different `kind` values use
different fields:

- `ND_NUM` uses only `val`.
- `ND_VAR` uses only `var`.
- `ND_ADD` uses `lhs` and `rhs`.
- `ND_NEG` uses only `lhs`.
- `ND_IF` uses `cond`, `then`, `els`.
- `ND_FOR` uses `init`, `cond`, `inc`, `then`.
- `ND_BLOCK` uses `body`.
- `ND_FUNCALL` uses `funcname` and `args`.

This is wasteful — most nodes use only 2-3 fields and the others are
unused — but it's much simpler than declaring a separate struct per
node kind, and the wastage is tiny in absolute terms.

The bottom of `mc.h` declares prototypes for the public functions:

```c
Token    *tokenize(char *p);
Function *parse(Token *tok);
void      add_type(Node *node);
void      codegen(Function *prog);
```

These are the four "stage entry points" in the pipeline.

## 3.2 `error.c` — friendly error messages

This file is small but important. When mc finds a syntax error in a
program, it should not just print "syntax error" — it should print the
source line, a caret pointing at the bad column, and a useful message,
like this:

```
3: int main() { return @ }
                       ^ unexpected character
```

Two functions do this.

`error()` is the dumb one — it just prints a message and exits:

```c
void error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}
```

`va_list`, `va_start`, `va_end` are the C facilities for variadic
functions (functions like `printf` that accept any number of
arguments). `vfprintf` is the version of `fprintf` that takes a
`va_list` instead of `...`.

`error()` is for catastrophic situations — out of memory, bad command
line arguments. For source errors, we use `error_at`:

```c
void error_at(char *loc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
    va_end(ap);
}
```

This thin wrapper calls `verror_at`, which does the real work. The
trick is that `loc` is a pointer back into the original source string
(the same `user_input` buffer that the lexer is walking through), so
we can:

1. Walk *back* from `loc` until we hit a newline or the start of the
   buffer — this gives us the start of the line.
2. Walk *forward* from `loc` until we hit a newline or end of buffer —
   this gives us the end of the line.
3. Print the line.
4. Print enough spaces to line up under `loc`'s column, then print
   `^`, then the formatted message.

```c
void verror_at(char *loc, const char *fmt, va_list ap) {
    char *line = loc;
    while (line > user_input && line[-1] != '\n')
        line--;

    char *end = loc;
    while (*end && *end != '\n')
        end++;

    int line_no = 1;
    for (char *p = user_input; p < line; p++)
        if (*p == '\n') line_no++;

    int indent = fprintf(stderr, "%d: ", line_no);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    int pos = (int)(loc - line) + indent;
    fprintf(stderr, "%*s", pos, "");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}
```

`%*s` with width `pos` and an empty string prints `pos` spaces — a
trick for indenting. After all the printing, we exit with code 1.

## 3.3 `tokenize.c` — turning text into tokens

The job: read a string, produce a linked list of tokens. The output
is the input to the parser.

The first three small helpers are exposed in `mc.h` so the parser can
call them:

```c
bool equal(Token *tok, const char *op);
Token *skip(Token *tok, const char *op);
int    get_number(Token *tok);
```

`equal(tok, "=")` returns true if the token's text is exactly `"="`.
`skip(tok, "(")` returns the next token if the current one is `"("`,
otherwise prints an error and exits. `get_number(tok)` returns the
integer value of a number token, or errors if the token isn't a
number.

These are cute little utilities that make the parser code read like
the grammar comment at the top.

The tokenizer itself is the function `tokenize`. Its core loop is a
big `while (*p)` over the input characters. At each iteration it asks:
"what kind of token starts here?"

```c
while (*p) {
    if (isspace((unsigned char)*p)) { p++; continue; }

    if (p[0] == '/' && p[1] == '/') { /* line comment */ ... }
    if (p[0] == '/' && p[1] == '*') { /* block comment */ ... }

    if (isdigit((unsigned char)*p)) { /* TK_NUM */ ... }
    if (*p == '"')                  { /* TK_STR */ ... }
    if (is_ident_start(*p))         { /* TK_IDENT */ ... }

    int len = read_punct(p);
    if (len)                        { /* TK_PUNCT */ ... }

    error_at(p, "invalid token");
}
```

The order matters. We first eat whitespace and comments. Then we test
"does this start with a digit? a quote? a letter or underscore?" in
that order. Finally we try to recognize punctuation. If nothing
matches, we report a tokenizer error.

For numbers, `strtol` from the standard library does the heavy
lifting:

```c
char *q = p;
long  v = strtol(p, &p, 10);
cur = cur->next = new_token(TK_NUM, q, p);
cur->val = (int)v;
```

`strtol` parses an integer starting at `p`, advances `p` past the
parsed digits, and returns the value. We remember where we started
(`q`) so we can record the token's location for error messages later.

For identifiers we walk while the character is a letter, digit, or
underscore:

```c
char *q = p;
do { p++; } while (is_ident_cont((unsigned char)*p));
cur = cur->next = new_token(TK_IDENT, q, p);
```

After all tokens are produced, a separate pass walks the list and
re-tags any identifier whose text matches a keyword (like `return` or
`while`) as `TK_KEYWORD`. This keeps the main loop simple — we don't
need to remember every keyword while we're tokenizing.

For string literals we have a small helper, `read_string_literal`,
that:

1. Walks to the closing `"`, refusing to cross a newline.
2. Allocates a buffer big enough for the result.
3. Walks again from the opening `"`, copying characters and handling
   escape sequences (`\n` → newline, `\t` → tab, etc.) via a
   `read_escape` helper.

For punctuation we look at one or two characters and decide. The
function `read_punct` first checks for two-character punctuation
(`==`, `!=`, `<=`, `>=`); failing that, if the current character is
*any* punctuation character, it's a one-character token.

The result of `tokenize` is a chain of `Token` structs ending with a
`TK_EOF` sentinel. The parser consumes this chain.

## 3.4 `parse.c` — turning tokens into a tree

This is the biggest file in the project (~500 lines). It has one
function per grammar rule, written in the *recursive descent* style.

At the top of the file is a big comment explaining the grammar, in
slightly informal notation. Here's a slimmed-down version:

```
program       = function-def*
function-def  = type ident "(" params? ")" "{" compound-stmt
params        = type ident ("," type ident)*
compound-stmt = (declaration | stmt)* "}"
declaration   = type declarator ("=" expr)? ("," ...)* ";"
declarator    = "*"* ident ("[" num "]")?
stmt          = "return" expr ";"
              | "if" "(" expr ")" stmt ("else" stmt)?
              | "while" "(" expr ")" stmt
              | "for" "(" expr-stmt expr? ";" expr? ")" stmt
              | "{" compound-stmt
              | expr-stmt
expr          = assign
assign        = equality ("=" assign)?
equality      = relational ("==" relational | "!=" relational)*
relational    = add ("<" add | "<=" add | ">" add | ">=" add)*
add           = mul ("+" mul | "-" mul)*
mul           = unary ("*" unary | "/" unary)*
unary         = ("+"|"-"|"&"|"*") unary | "sizeof" unary | postfix
postfix       = primary ("[" expr "]")*
primary       = num | ident ("(" args? ")")? | "(" expr ")" | string
```

Read that top-down. The whole *program* is a list of *function-defs*.
Each function-def starts with a type (`int` or `char`), an identifier,
a parenthesised parameter list, and a `{ ... }` body. The body is a
*compound-stmt*, which is a sequence of declarations and statements
ended with `}`.

Each grammar rule becomes a C function with the same name. For
example:

```c
static Node *equality(Token **rest, Token *tok) {
    Node *node = relational(&tok, tok);
    for (;;) {
        if (equal(tok, "==")) { node = new_binary(ND_EQ, node, relational(&tok, tok->next)); continue; }
        if (equal(tok, "!=")) { node = new_binary(ND_NE, node, relational(&tok, tok->next)); continue; }
        *rest = tok;
        return node;
    }
}
```

This implements the rule `equality = relational ("==" relational |
"!=" relational)*`. Read it as: parse one `relational`; while the
current token is `==` or `!=`, consume it and parse another
`relational`, combining them into an ND_EQ or ND_NE node.

The *layered* style — one function per precedence level — is what
gives mc its operator precedence rules. `mul` calls `unary`, then
loops on `*` or `/`. `add` calls `mul`, then loops on `+` or `-`.
That's why `1 + 2 * 3` parses as `1 + (2 * 3)` and not `(1 + 2) * 3`:
the `mul` for `2 * 3` runs *before* `add` sees the `+`.

The two-pointer convention `(Token **rest, Token *tok)` is a chibicc
idiom. The function reads tokens starting at `tok`. After it's done,
it writes the token it stopped at into `*rest`, so the caller can
continue from there. This is the same as a C++ "iterator passed by
reference" idiom; we just spell it out.

`parse_params` consumes the parameter list of a function. It calls
`declspec` (which handles `int` or `char`) once per parameter and
adds each parameter to the function's `locals` list via `new_lvar`.

`function` is the per-function entry point:

```c
static Function *function(Token **rest, Token *tok) {
    if (!is_typename(tok))
        error_at(tok->loc, "expected a type name at start of function");
    declspec(&tok, tok);

    if (tok->kind != TK_IDENT)
        error_at(tok->loc, "expected function name");
    char *name = strndup_(tok->loc, tok->len);
    tok = tok->next;

    locals = NULL;

    tok = skip(tok, "(");
    int n_params = parse_params(&tok, tok);
    tok = skip(tok, ")");
    tok = skip(tok, "{");

    Function *fn = calloc(1, sizeof(Function));
    fn->name     = name;
    fn->n_params = n_params;
    fn->body     = compound_stmt(&tok, tok);
    fn->locals   = locals;
    assign_lvar_offsets(fn);
    *rest = tok;
    return fn;
}
```

`locals = NULL` resets the symbol table at the start of every
function — each function has its own independent locals.

`assign_lvar_offsets` walks the locals list once and gives each one a
stack offset. The first local goes at `-8(%rbp)` if it's 8 bytes, and
so on. The total is rounded up to 16 bytes for the System V alignment
rule.

`declaration` handles `int x = 3;` and `int *p, q;` and `int a[10];`:

```c
static Node *declaration(Token **rest, Token *tok) {
    Type *base = declspec(&tok, tok);
    Node head = {0};
    Node *cur = &head;
    int  i = 0;

    while (!equal(tok, ";")) {
        if (i++ > 0) tok = skip(tok, ",");
        Type *ty = base;
        while (equal(tok, "*")) { ty = pointer_to(ty); tok = tok->next; }

        if (tok->kind != TK_IDENT) error_at(tok->loc, "expected variable name");
        char *name = strndup_(tok->loc, tok->len);
        tok = tok->next;

        if (equal(tok, "[")) {
            tok = tok->next;
            int len = get_number(tok);
            tok = skip(tok->next, "]");
            ty = array_of(ty, len);
        }

        Obj *var = new_lvar(name, ty);

        if (!equal(tok, "=")) continue;

        Node *lhs = new_var_node(var);
        Node *rhs = assign(&tok, tok->next);
        Node *assn = new_binary(ND_ASSIGN, lhs, rhs);
        cur = cur->next = new_unary(ND_EXPR_STMT, assn);
    }

    Node *node = new_node(ND_BLOCK);
    node->body = head.next;
    *rest = tok->next;
    return node;
}
```

For each declarator, we eat any leading `*`s (each one wraps the type
in a pointer), parse the identifier, optionally parse a `[N]` array
suffix, then optionally parse an `= expr` initializer. The
initializer is *lowered* into an assignment expression statement, so
codegen doesn't need a separate "declarator with init" node kind. The
declaration as a whole becomes an `ND_BLOCK` whose body is the chain
of expression-statements.

`stmt` dispatches on keywords to handle `return`, `if`, `while`,
`for`, blocks, and falls through to `expr_stmt` otherwise.

`primary` is the deepest expression rule:

```c
static Node *primary(Token **rest, Token *tok) {
    if (equal(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }
    if (tok->kind == TK_NUM) {
        Node *node = new_num(tok->val);
        *rest = tok->next;
        return node;
    }
    if (tok->kind == TK_STR) {
        Obj *str = new_string_literal(tok->str, tok->str_len);
        *rest = tok->next;
        return new_var_node(str);
    }
    if (tok->kind == TK_IDENT) {
        if (equal(tok->next, "(")) {
            // function call
            ...
        }
        // variable reference
        Obj *v = find_var(tok);
        if (!v) v = new_lvar(strndup_(tok->loc, tok->len), ty_int);
        *rest = tok->next;
        return new_var_node(v);
    }
    error_at(tok->loc, "expected an expression");
    return NULL;
}
```

Notice: if the user references an identifier that isn't already a
known variable, mc *auto-declares* it as an int. This is a chibicc-
inherited convenience that lets you write `a = 3; return a;` without
saying `int a;` first. It's not real C, but it makes the test suite
shorter.

A *string literal* in `primary` triggers `new_string_literal`, which
creates a fresh anonymous global with a name like `.L.str.0`, type
`char[len+1]` (with the trailing `\0`), and the literal bytes. The
expression becomes an `ND_VAR` referring to that global.

## 3.5 `type.c` — giving every expression a type

After the parser builds the AST, every node has `kind`, `lhs`, `rhs`,
etc., but no `ty`. The type pass fills in `ty` and rewrites pointer
arithmetic.

`add_type` is one big recursion: descend into all child nodes first,
then figure out the current node's type:

```c
void add_type(Node *node) {
    if (!node || node->ty) return;

    add_type(node->lhs);
    add_type(node->rhs);
    add_type(node->cond);
    add_type(node->then);
    add_type(node->els);
    add_type(node->init);
    add_type(node->inc);

    for (Node *n = node->body; n; n = n->next) add_type(n);
    for (Node *n = node->args; n; n = n->next) add_type(n);

    switch (node->kind) {
        ...
    }
}
```

The early return on `node->ty` makes the pass *idempotent* — calling
it twice on the same tree is safe. It's also how `sizeof` in the
parser works: `sizeof` calls `add_type` on its operand subtree, then
folds the size into a constant; if codegen later runs `add_type` on
the whole function body, it just re-confirms everything.

The interesting cases:

- `ND_NUM`: `node->ty = ty_int;`
- `ND_VAR`: `node->ty = node->var->ty;` — copy the variable's type.
- `ND_ASSIGN`: `node->ty = node->lhs->ty;` — the assignment has the
  type of the left side.
- `ND_ADDR`: a tricky one. If the operand is an array, the result
  type is "pointer to element"; otherwise it's "pointer to operand
  type". This is the array-decay rule for `&arr` and is part of why
  `&a[0]` and `a` are interchangeable.
- `ND_DEREF`: if the operand is pointer-like (a pointer or an array),
  the result is the element type; otherwise we fall back to int.
- `ND_ADD`: if both operands are integers, the result is int. If
  either operand is pointer-like, we **rewrite the tree**: scale the
  integer operand by `sizeof(*ptr)` so that codegen can stay
  scalar-only. The result type is "pointer to element".
- `ND_SUB`: same idea for pointer-minus-int and pointer-minus-pointer.

Look at the rewrite for `ND_ADD`:

```c
if (is_pointer_like(node->lhs->ty) && is_integer(node->rhs->ty)) {
    int sz = size_of(node->lhs->ty->base);
    Node *scale = calloc(1, sizeof(Node));
    scale->kind = ND_MUL;
    scale->lhs  = node->rhs;
    Node *k = calloc(1, sizeof(Node));
    k->kind = ND_NUM;
    k->val  = sz;
    k->ty   = ty_int;
    scale->rhs = k;
    scale->ty  = ty_int;
    node->rhs = scale;
    node->ty  = pointer_to(node->lhs->ty->base);
    return;
}
```

If we have `p + n` (pointer + int), we replace `n` with a fresh
`ND_MUL` node whose left child is the original `n` and right child is
a constant `sizeof(*p)`. After the rewrite, the addition's right
operand is `n * sizeof(*p)`, which codegen then computes plainly.
This is the trick that lets the rest of the compiler ignore pointer
arithmetic entirely.

`size_of(ty)` returns the size in bytes of any type:

```c
int size_of(Type *ty) {
    if (!ty)                    return 8;
    if (ty->kind == TY_CHAR)    return 1;
    if (ty->kind == TY_INT)     return 8;
    if (ty->kind == TY_PTR)     return 8;
    if (ty->kind == TY_ARRAY)   return size_of(ty->base) * ty->array_len;
    return 8;
}
```

We treat `int` as 8 bytes (same as a pointer), even though real C
treats it as 4. This is a deliberate simplification, and the few tests
that rely on `sizeof(int) == 8` carry a `// no-diff` marker so they
get skipped from the differential test against real gcc.

## 3.6 `codegen.c` — turning the tree into assembly

Codegen walks the typed AST and emits assembly to standard output.
Every expression is computed into `%rax`. Sub-expression results are
saved on the stack with `push %rax` and recovered with `pop %rdi`.

A handful of helpers anchor the file:

```c
static int depth;                          // # of values pushed
static const char *current_fn;             // name of current function

static int count(void) { static int i = 1; return i++; }

static void push(void) { printf("  push %%rax\n"); depth++; }
static void pop (const char *r) { printf("  pop %s\n", r); depth--; }
```

`depth` counts how many 8-byte values are on the expression stack
right now. The codegen asserts `depth == 0` between statements. This
counter also drives the alignment-padding logic at function calls.
`count()` returns a fresh integer every time we need a unique label
(`.L.else.7`, etc.).

`gen_addr` produces the *address* of an lvalue. There are two kinds:

```c
static void gen_addr(Node *node) {
    if (node->kind == ND_VAR) {
        if (node->var->is_local)
            printf("  lea %d(%%rbp), %%rax\n", node->var->offset);
        else
            printf("  lea %s(%%rip), %%rax\n", node->var->name);
        return;
    }
    if (node->kind == ND_DEREF) {
        gen_expr(node->lhs);
        return;
    }
    error("not an lvalue");
}
```

For a local, we use a stack-relative `lea`. For a global (including
string literals), we use a *RIP-relative* `lea` — the address is
"this label, relative to the current instruction pointer", which is
how PIE (position-independent executable) binaries reference global
data. For `*p` as an lvalue (`*p = 3`), the address is just whatever
`p` evaluates to, so we generate `p` as an expression.

`load` and `store` are width-aware:

```c
static void load(Type *ty) {
    if (ty && ty->kind == TY_ARRAY) return;
    if (ty && ty->kind == TY_CHAR)
        printf("  movsbq (%%rax), %%rax\n");
    else
        printf("  mov (%%rax), %%rax\n");
}
```

If the type is an *array*, we don't load anything — the address is
the value (this is the array-to-pointer decay we promised in the type
pass). For `char`, we use `movsbq`, which sign-extends a byte to 64
bits. For everything else we use a full 8-byte `mov`.

`gen_expr` is the big switch:

```c
static void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        printf("  mov $%d, %%rax\n", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        printf("  neg %%rax\n");
        return;
    case ND_VAR:
        gen_addr(node);
        load(node->ty);
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        pop("%rdi");
        store(node->lhs->ty);
        return;
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        load(node->ty);
        return;
    case ND_FUNCALL: {
        // ... see below
    }
    default:
        break;
    }

    // binary case (ND_ADD, ND_SUB, ND_MUL, ND_DIV, comparisons)
    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("%rdi");

    switch (node->kind) {
    case ND_ADD: printf("  add %%rdi, %%rax\n"); return;
    case ND_SUB: printf("  sub %%rdi, %%rax\n"); return;
    case ND_MUL: printf("  imul %%rdi, %%rax\n"); return;
    case ND_DIV:
        printf("  cqo\n");           // sign-extend %rax into %rdx:%rax
        printf("  idiv %%rdi\n");    // signed divide by %rdi
        return;
    case ND_EQ: case ND_NE: case ND_LT: case ND_LE:
        printf("  cmp %%rdi, %%rax\n");
        if (node->kind == ND_EQ) printf("  sete  %%al\n");
        if (node->kind == ND_NE) printf("  setne %%al\n");
        if (node->kind == ND_LT) printf("  setl  %%al\n");
        if (node->kind == ND_LE) printf("  setle %%al\n");
        printf("  movzb %%al, %%rax\n");
        return;
    default:
        error("invalid expression node");
    }
}
```

Read the binary case slowly. For `a + b`:

1. Compute `b` into `%rax`.
2. `push %rax` (depth++).
3. Compute `a` into `%rax`.
4. `pop %rdi` (depth--), so `%rdi` now holds `b`'s value.
5. `add %rdi, %rax`. Now `%rax = a + b`.

For comparisons, we use `cmp` (which sets condition flags but doesn't
touch its operands), then `setcc` (which writes 0 or 1 to a single
byte based on the condition), then `movzb` (which zero-extends that
byte to 64 bits).

The function-call case is where the alignment-pad fix lives:

```c
case ND_FUNCALL: {
    int nargs = 0;
    for (Node *a = node->args; a; a = a->next) {
        gen_expr(a);
        push();
        nargs++;
    }
    if (nargs > 6) error("too many arguments to %s", node->funcname);
    for (int i = nargs - 1; i >= 0; i--)
        pop(argreg[i]);

    if (depth % 2) {
        printf("  sub $8, %%rsp\n");
        printf("  mov $0, %%rax\n");
        printf("  call %s\n", node->funcname);
        printf("  add $8, %%rsp\n");
    } else {
        printf("  mov $0, %%rax\n");
        printf("  call %s\n", node->funcname);
    }
    return;
}
```

Each argument is computed and pushed left-to-right. After all are
pushed, we pop them in reverse, so the *first* argument ends up in
`%rdi`, the second in `%rsi`, etc. Then comes the alignment check —
if `depth` is odd at the call site, `%rsp` is 8 off from a multiple of
16, so we emit `sub $8, %rsp` before the call and `add $8, %rsp` after
to balance it.

`mov $0, %rax` before each call sets `%al` to 0, which the System V
ABI requires for variadic calls (it tells the callee how many vector
arguments are passed; we pass none).

`gen_stmt` handles statement-level nodes:

```c
static void gen_stmt(Node *node) {
    switch (node->kind) {
    case ND_RETURN:
        gen_expr(node->lhs);
        printf("  jmp .L.return.%s\n", current_fn);
        return;
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next) gen_stmt(n);
        return;
    case ND_IF: {
        int c = count();
        gen_expr(node->cond);
        printf("  cmp $0, %%rax\n");
        printf("  je  .L.else.%d\n", c);
        gen_stmt(node->then);
        printf("  jmp .L.end.%d\n", c);
        printf(".L.else.%d:\n", c);
        if (node->els) gen_stmt(node->els);
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_FOR: {
        int c = count();
        if (node->init) gen_stmt(node->init);
        printf(".L.begin.%d:\n", c);
        if (node->cond) {
            gen_expr(node->cond);
            printf("  cmp $0, %%rax\n");
            printf("  je  .L.end.%d\n", c);
        }
        gen_stmt(node->then);
        if (node->inc) gen_expr(node->inc);
        printf("  jmp .L.begin.%d\n", c);
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    default:
        error("invalid statement");
    }
}
```

`if` becomes: evaluate the condition, compare to 0; if equal, jump to
the else label. Otherwise execute the `then` block, then jump past
the else. The `count()` ensures every `if` in a function gets unique
labels.

`for` has the same shape as `while` (a `while` is a `for` with no
init or inc). We emit a "begin" label, evaluate the condition, jump
to "end" if it's false, otherwise run the body, run the increment,
jump back to "begin".

The top-level `codegen` puts it all together:

```c
void codegen(Function *prog) {
    for (Function *fn = prog; fn; fn = fn->next)
        add_type(fn->body);

    emit_data();                     // .data section for globals
    printf("  .text\n");

    for (Function *fn = prog; fn; fn = fn->next) {
        printf("  .globl %s\n", fn->name);
        printf("%s:\n", fn->name);
        current_fn = fn->name;

        printf("  push %%rbp\n");
        printf("  mov %%rsp, %%rbp\n");
        if (fn->stack_size)
            printf("  sub $%d, %%rsp\n", fn->stack_size);

        // spill register args to their stack slots
        ...

        for (Node *n = fn->body; n; n = n->next) {
            gen_stmt(n);
            assert(depth == 0);
        }

        printf("  mov $0, %%rax\n");
        printf(".L.return.%s:\n", fn->name);
        printf("  mov %%rbp, %%rsp\n");
        printf("  pop %%rbp\n");
        printf("  ret\n");
    }
}
```

For each function we:

1. Emit the function label and prologue.
2. Spill register arguments into their assigned stack slots.
3. Generate the body.
4. Emit the epilogue.

The spill loop is what bug #2 fixed (see [section 6.2](#62-bug-2-parameter-spill-targeted-the-wrong-slots)).

`emit_data` prints all globals (mostly string literals) into the
`.data` section as `.byte` values, before the `.text` section starts.

## 3.7 `main.c` — the front door

This file is small. It reads the input file into a buffer and calls
the four pipeline stages in order.

```c
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: mc [--dump-tokens] <file.c>\n");
        return 1;
    }

    if (strcmp(argv[1], "--dump-tokens") == 0) {
        ...
        return dump_tokens(argv[2]);
    }

    char     *src  = read_file(argv[1]);
    Token    *tok  = tokenize(src);
    Function *prog = parse(tok);
    codegen(prog);
    return 0;
}
```

`read_file` is a portable file-slurp that uses `fopen`/`fread`/`fseek`
rather than POSIX-only `getline`, so the compiler builds with MinGW on
Windows too.

The optional `--dump-tokens` flag is a debug helper: it tokenizes the
input and prints each token to stderr. Useful when you suspect the
lexer is misbehaving.

---

# 4. The Makefile and the build system

`make` is a tool that knows how to rebuild a project incrementally —
it only recompiles files that have changed. The `Makefile` describes
the dependency graph.

```makefile
CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O0 -g
SRCS    := $(wildcard src/*.c)
OBJS    := $(SRCS:.c=.o)
TARGET  := mc

.PHONY: all clean test diff_test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

src/%.o: src/%.c src/mc.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TARGET)
	@bash test/test.sh

diff_test: $(TARGET)
	@bash test/diff_test.sh

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe tmp.s tmp.out tmp.exe err
```

What this says:

- `CC` is the C compiler we use to build mc itself (gcc).
- `CFLAGS` are the flags. `-std=c11` requests C11, `-Wall -Wextra` ask
  for thorough warnings, `-O0` disables optimisation (faster build,
  better debugging), `-g` includes debug symbols.
- `SRCS := $(wildcard src/*.c)` collects every `.c` file in `src/`.
- `OBJS := $(SRCS:.c=.o)` is the same list but with `.c` replaced by
  `.o` — these are the object files we'll produce.
- The rule `$(TARGET): $(OBJS)` says "to build `mc`, you need all the
  object files; then run gcc to link them together."
- The pattern rule `src/%.o: src/%.c src/mc.h` says "any `.o` file
  depends on the corresponding `.c` file *and* on `mc.h`. To build
  one, run gcc with `-c` (compile, don't link)."
- `test` and `diff_test` are *phony* targets — they don't produce a
  file, they just run a script.
- `clean` removes everything you built.

`make` reads the rules and figures out the right order: if you change
`src/parse.c`, `make` will re-run the compile step for `parse.o` and
then re-link `mc`, but it won't recompile `tokenize.o` because
`tokenize.c` hasn't changed.

---

# 5. A worked example, end to end

Let's trace the C program

```c
int main() { return 1 + 2 * 3; }
```

through the entire compiler. The expected exit code is `7`.

## 5.1 What the lexer produces

The tokenizer reads the string and produces this sequence of tokens:

| Token text | Kind        | Notes                          |
| ---------- | ----------- | ------------------------------ |
| `int`      | TK_KEYWORD  | (re-tagged from TK_IDENT)      |
| `main`     | TK_IDENT    |                                |
| `(`        | TK_PUNCT    |                                |
| `)`        | TK_PUNCT    |                                |
| `{`        | TK_PUNCT    |                                |
| `return`   | TK_KEYWORD  |                                |
| `1`        | TK_NUM      | val = 1                        |
| `+`        | TK_PUNCT    |                                |
| `2`        | TK_NUM      | val = 2                        |
| `*`        | TK_PUNCT    |                                |
| `3`        | TK_NUM      | val = 3                        |
| `;`        | TK_PUNCT    |                                |
| `}`        | TK_PUNCT    |                                |
| (eof)      | TK_EOF      |                                |

You can see this directly with `./mc --dump-tokens hello.c`.

## 5.2 What the parser produces

The parser starts at the top with `parse`, which calls `function`:

- Sees `int`, knows it's a function.
- Reads `main`, sets the function name.
- Sees `(` `)`, parses zero parameters.
- Sees `{`, calls `compound_stmt`.
- `compound_stmt` keeps reading statements until `}`. There's exactly
  one: a `return` statement.
- `stmt` sees `return`, calls `expr` to parse `1+2*3`.
- `expr` calls `assign` calls `equality` calls `relational` calls
  `add`.
- `add` calls `mul` first (to parse `1`).
  - `mul` calls `unary` calls `postfix` calls `primary`. `primary`
    sees `1` and returns an `ND_NUM(1)`.
  - `mul` returns to `add` with the `ND_NUM(1)` because the next
    token is `+`, not `*` or `/`.
- `add` sees `+`. It calls `mul` again to parse the right-hand side.
  - `mul` calls `unary`/`postfix`/`primary`, gets `ND_NUM(2)`.
  - `mul` sees `*`, calls `unary`/`postfix`/`primary` again, gets
    `ND_NUM(3)`.
  - `mul` builds `ND_MUL(ND_NUM(2), ND_NUM(3))` and returns it.
- `add` builds `ND_ADD(ND_NUM(1), ND_MUL(ND_NUM(2), ND_NUM(3)))`.
- `stmt` wraps that in `ND_RETURN`.
- `compound_stmt` wraps that in `ND_BLOCK`.

The resulting tree:

```
                 ND_BLOCK
                    │
                    ▼
                ND_RETURN
                    │
                    ▼
                  ND_ADD
                  /    \
              ND_NUM(1)  ND_MUL
                          /    \
                      ND_NUM(2)  ND_NUM(3)
```

This tree captures the precedence (`*` binds tighter than `+`)
correctly because `mul` ran inside `add` before `add` saw the `+`.

## 5.3 What the type pass does

`add_type` walks the tree:

- `ND_NUM(1).ty = ty_int`.
- `ND_NUM(2).ty = ty_int`.
- `ND_NUM(3).ty = ty_int`.
- `ND_MUL`'s case: both children are `ty_int`, so the result is
  `ty_int`.
- `ND_ADD`'s case: both children are `ty_int`, so the result is
  `ty_int`. No pointer rewrite is needed.

The tree's shape doesn't change for this program. (It would, if we
had `p + 1` where `p` was a pointer.)

## 5.4 What codegen emits

Walking the tree:

- `codegen` emits the function label and prologue:

  ```
    .text
    .globl main
  main:
    push %rbp
    mov %rsp, %rbp
  ```

  No `sub $N, %rsp` because main has no locals.

- `gen_stmt(ND_BLOCK)` walks its body and calls `gen_stmt` on
  `ND_RETURN`.
- `gen_stmt(ND_RETURN)` calls `gen_expr` on the return value, then
  emits `jmp .L.return.main`.
- `gen_expr(ND_ADD)`:
  - `gen_expr(rhs = ND_MUL(2,3))`:
    - `gen_expr(rhs = 3)` → `mov $3, %rax`.
    - `push` → `push %rax`.
    - `gen_expr(lhs = 2)` → `mov $2, %rax`.
    - `pop "%rdi"` → `pop %rdi`.
    - emit `imul %rdi, %rax` → `%rax = 6`.
  - `push` → `push %rax`.
  - `gen_expr(lhs = 1)` → `mov $1, %rax`.
  - `pop "%rdi"` → `pop %rdi`.
  - emit `add %rdi, %rax` → `%rax = 7`.
- `gen_stmt(ND_RETURN)` then emits the jump:

  ```
    jmp .L.return.main
  ```

- After the body, codegen emits the trailing zero-return and the
  epilogue:

  ```
    mov $0, %rax
  .L.return.main:
    mov %rbp, %rsp
    pop %rbp
    ret
  ```

The full output is what you saw in section 1.3.

## 5.5 What gas turns it into

`gcc out.s -o out` invokes `as` (the GNU assembler) with `out.s` as
input. `as` does three things:

1. Parses the text of every instruction.
2. Encodes each instruction into the binary machine-code form. For
   instance, `mov $1, %rax` becomes the bytes `48 c7 c0 01 00 00 00`.
3. Records every label and reference so the linker can resolve them.

The result is `out.o`, an *object file* — binary, but missing things
like the C runtime startup (`_start`) and the `_init`/`_fini` glue.

Then `ld` (the linker, called by gcc behind the scenes) takes
`out.o`, plus the standard CRT object files and libc, and stitches
them into a final executable, `out`.

## 5.6 What the kernel does

When you run `./out`, the Linux kernel:

1. Reads the ELF (executable) header to find the entry point. For a
   typical gcc-produced binary, that's `_start` from the C runtime.
2. Maps the executable's code and data into memory.
3. Sets up an initial stack with the argv / environ.
4. Jumps to `_start`.

`_start` does some setup (initialises libc, parses argv) and then
calls `main`. Our `main` runs, computes 7 into `%rax`, and `ret`s back
to `_start`. The CRT then calls `exit(7)`, which is a syscall that
asks the kernel to terminate the process and store the exit code.

If you then type `echo $?` in the shell, the shell prints `7`. That's
the value our program returned.

---

# 6. The two real bugs we fixed

Both of these bugs were invisible during local development on Windows
(because mc emits Linux assembly, and Windows can't run Linux ELF).
They were caught only when CI tried to actually run the assembly on
Ubuntu.

## 6.1 Bug 1: stack misalignment at mid-expression calls

The System V ABI requires the stack pointer (`%rsp`) to be a multiple
of 16 at the moment of every `call` instruction. The function
prologue ensures this when entering a function, but during expression
evaluation, the stack temporarily holds intermediate results, and
this can put `%rsp` 8 off.

Specifically: imagine the program

```c
return fib(n-1) + fib(n-2);
```

The codegen for `+` is:

1. Compute the right operand (`fib(n-2)`). At this point `depth = 0`,
   so `%rsp` is aligned. The call to `fib` is fine.
2. Push the result: `push %rax`. Now `depth = 1`, `%rsp` is off by 8.
3. Compute the left operand (`fib(n-1)`). When we reach the `call`
   inside this, `depth = 1` — `%rsp` is misaligned.

Calling `fib` with a misaligned `%rsp` is mostly fine, but if `fib`
itself indirectly uses any SIMD instructions (and glibc and many
runtime libraries do), the program crashes with a segmentation fault.

The fix: at every call site, check whether `depth` is odd. If so,
emit `sub $8, %rsp` before the call and `add $8, %rsp` after. The
extra 8 bytes restore alignment for the call and are reclaimed
afterwards.

```c
if (depth % 2) {
    printf("  sub $8, %%rsp\n");
    printf("  mov $0, %%rax\n");
    printf("  call %s\n", node->funcname);
    printf("  add $8, %%rsp\n");
} else {
    printf("  mov $0, %%rax\n");
    printf("  call %s\n", node->funcname);
}
```

In `case_func_03.c` (the `fib(10)` test), this means only one of the
three `call fib` instructions is padded — the one inside
`fib(n-1) + fib(n-2)` where the right operand has already been
pushed.

## 6.2 Bug 2: parameter spill targeted the wrong slots

When a function with parameters runs, its prologue must copy the
argument registers (`%rdi`, `%rsi`, ...) into the stack slots
allocated for those parameters. The parameters and the local
variables live in the same `Obj` linked list (`fn->locals`).

`parse_params` calls `new_lvar` for each parameter, in source order.
`new_lvar` pushes onto the *front* of `locals`. So after
`parse_params`, the list looks like (for two params `s` and `n`):

```
locals: [n, s, NULL]
```

(Reverse-source order, because each `new_lvar` prepends.) The
prologue's spill loop assumes that `locals[0..n_params-1]` are the
parameters, in reverse-source order — which works perfectly *as long
as the body doesn't add any locals*.

But if the function body has a declaration like `int n = 0;`, that
declaration calls `new_lvar` too, and pushes onto the front:

```
locals: [n_body, s_param, n_param, NULL]
```

Now the first `n_params` entries of locals are not the parameters at
all — they're the body locals. The spill loop, applied unchanged,
writes `%rdi` (the value of `s`) into `n_body`'s slot.

This is exactly what was happening in `case_str_02.c`:

```c
int strlen_(char *s) {
    int n = 0;
    ...
}
```

When `main` called `strlen_("hello")`, `%rdi` held the address of
the string. The spill code wrote it into `n`'s slot, and `s`'s slot
stayed garbage. Then the loop body tried to dereference `s` and read
some random byte, returning a wrong length.

The fix: walk to the *tail* of `locals` first, then iterate through
the params. Since `parse_params` runs before any body declarations,
the parameters always end up at the tail.

```c
int total = 0;
for (Obj *o = fn->locals; o; o = o->next) total++;
int skip = total - fn->n_params;
Obj *p = fn->locals;
for (int i = 0; i < skip; i++) p = p->next;
for (int i = 0; i < fn->n_params; i++) {
    int src_index = fn->n_params - 1 - i;
    printf("  mov %s, %d(%%rbp)\n", argreg[src_index], p->offset);
    p = p->next;
}
```

After this fix, all 47 regression tests and all 35 differential tests
pass.

---

# 7. Running it yourself

You need a Linux environment. On Windows, the cleanest way is
**Windows Subsystem for Linux** (WSL):

```
wsl --install -d Ubuntu          # one-time setup; takes ~5 minutes
```

After Ubuntu is installed:

```bash
sudo apt-get update
sudo apt-get install -y gcc make
git clone https://github.com/Shinchan493/mc-compiler.git
cd mc-compiler
make             # builds the compiler binary 'mc'
```

To compile and run a program:

```bash
echo 'int main() { return 1 + 2 * 3; }' > hello.c
./mc hello.c > hello.s
gcc hello.s -o hello
./hello
echo $?           # prints 7
```

To run the test suite:

```bash
make test         # 47 regression tests
make diff_test    # 35 differential tests
```

---

# 8. Glossary

**ABI** — *Application Binary Interface*. The contract between two
pieces of compiled code (e.g. a caller and a callee) about how
arguments are passed, where return values go, what registers can be
clobbered.

**AST** — *Abstract Syntax Tree*. A tree representation of source
code's structure. Internal nodes are operations; leaves are literals
and variables.

**assembly** — A human-readable text form of machine code. Each line
is one CPU instruction. Several flavours exist; mc uses AT&T / GAS
syntax.

**big-endian / little-endian** — Two ways to lay out the bytes of a
multi-byte number in memory. x86-64 is little-endian. Doesn't come up
in mc directly, but you'll see it elsewhere.

**call stack** — The runtime data structure that holds one *frame*
per active function call. Each frame stores the function's locals
and saved registers. Grows downward on x86-64.

**chibicc** — Rui Ueyama's small C compiler. mc's architecture is
heavily inspired by it.

**CI** (Continuous Integration) — Automation that builds and tests
your code every time you push.

**codegen** — Code generation, the stage of a compiler that produces
the output language (in mc's case, x86-64 assembly).

**compiler** — A program that translates one language into another.
mc's input language is C; mc's output language is x86-64 assembly.

**directive** — In assembly, a line that controls the assembler
itself rather than producing an instruction (e.g. `.text`,
`.globl`).

**ELF** — Executable and Linkable Format, the standard binary format
for executables on Linux.

**epilogue / prologue** — The fixed pieces of code at the start and
end of every function: prologue sets up the stack frame; epilogue
tears it down.

**gas** — GNU assembler. Turns `.s` files into `.o` files.

**gcc** — GNU C Compiler. mc uses gcc to assemble and link the
assembly it emits, but the actual translation from C → assembly is
mc's own job.

**immediate** — A literal numeric value embedded in an instruction
(e.g. the `5` in `mov $5, %rax`).

**label** — A name attached to a position in the program. Used as the
target of `jmp` and `call`.

**lexer** / **tokenizer** — The compiler stage that breaks input text
into tokens.

**linked list** — A data structure where each element points to the
next. mc uses these for tokens, locals, functions, and globals.

**linker** — The tool that combines object files into an executable.

**lvalue / rvalue** — An *lvalue* is something that can appear on the
left of `=` (a variable, `*p`). An *rvalue* is something that can
only be read.

**Makefile** — A file that describes how to build a project. Read by
`make`.

**MinGW** — A Windows port of gcc. Used during local development on
Windows to build mc itself.

**object file** — A `.o` file. Machine code, but not yet a complete
program; needs the linker.

**parser** — The compiler stage that builds an AST from a token
stream.

**PIE** — *Position-Independent Executable*. An executable that can
be loaded at any address; required for ASLR (a security feature).
Modern gcc produces PIE by default.

**precedence** — Which operator binds tighter than another. `*`
binds tighter than `+`, so `1 + 2 * 3 = 7`, not `9`.

**recursive descent** — A parser style where one function per
grammar rule directly calls the functions for sub-rules.

**register** — A small piece of storage inside the CPU itself. mc
uses `%rax` for results, `%rdi`/`%rsi`/etc for arguments, `%rsp` for
the stack pointer, `%rbp` for the base pointer.

**RIP-relative addressing** — Referring to a memory address by
"this label, relative to the current instruction pointer (`%rip`)".
PIE-friendly.

**spill** — Writing a register's value to memory because we need the
register for something else, then reading it back later.

**stack frame** — One activation of a function's stack space:
locals, saved `%rbp`, saved registers.

**stack machine** — A code-generation strategy where every value
goes onto a stack rather than into a register. mc's codegen is a
stack machine that uses the CPU's actual stack.

**System V ABI** — The calling convention used by Linux and macOS on
x86-64.

**token** — One indivisible word or symbol from the source. The
lexer's output unit.

**type** — In C, a label that says how to interpret a value: as an
integer, a pointer, an array, etc. mc has four type kinds.

**WSL** — *Windows Subsystem for Linux*. Lets you run Linux binaries
on Windows.

---

If you've made it here, you understand mc. Read the source — it'll
make sense now.
