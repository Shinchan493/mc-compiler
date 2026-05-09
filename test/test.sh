#!/usr/bin/env bash
# Regression test harness for mc.
#
# Each test/case_*.c starts with a comment of the form
#     // expect=<integer>
# giving the expected exit code. We compile with mc, assemble+link with
# gcc -static, run the resulting binary, and compare exit codes.
#
# This script only works on Linux (the assembly mc emits is x86-64
# System V). On Windows we still build the compiler — we just can't run
# the test suite end-to-end.

set -u
pass=0
fail=0
failed=()

MC=./mc
[ -x "$MC" ] || MC=./mc.exe

# When CI is debugging an unknown regression failure, we want the
# log to include the *first* failing case in full detail. This var
# is consumed below; flip to "1" for permanent verbosity.
VERBOSE_FAILURES=${MC_VERBOSE:-1}

for src in test/case_*.c; do
  [ -e "$src" ] || continue
  expected=$(grep -oE 'expect=-?[0-9]+' "$src" | head -1 | cut -d= -f2)
  if [ -z "$expected" ]; then
    echo "SKIP no expect= directive in $src"
    continue
  fi

  if ! "$MC" "$src" > tmp.s 2>err; then
    echo "FAIL compile: $src"
    sed 's/^/    /' err
    fail=$((fail+1)); failed+=("$src"); continue
  fi
  if ! gcc tmp.s -o tmp.out 2>err; then
    echo "FAIL assemble: $src"
    sed 's/^/    /' err
    fail=$((fail+1)); failed+=("$src"); continue
  fi

  ./tmp.out
  got=$?
  if [ "$got" = "$expected" ]; then
    pass=$((pass+1))
  else
    echo "FAIL $src: expected $expected got $got"
    fail=$((fail+1)); failed+=("$src")
    if [ "$VERBOSE_FAILURES" = "1" ] && [ "$fail" = "1" ]; then
      echo "  --- mc-emitted assembly (first failure) ---"
      sed 's/^/    /' tmp.s
      echo "  --- /proc/self/exe-style: file tmp.out ---"
      file tmp.out 2>/dev/null || true
    fi
  fi
done

rm -f tmp.s tmp.out err
echo "passed $pass / $((pass+fail))"
[ "$fail" -eq 0 ]
