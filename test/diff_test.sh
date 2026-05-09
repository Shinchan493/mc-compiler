#!/usr/bin/env bash
# Differential test harness for mc.
#
# For each test/case_*.c, compile it with BOTH mc and 'gcc -O0',
# run the resulting binaries, and verify they produce identical
# exit codes AND identical stdout. This catches a whole class of
# bugs that the simple "expected exit code" regression check misses
# (wrong-but-consistent results, incorrect stdout, et cetera).
#
# Tests that legitimately diverge from gcc — typically because they
# depend on mc's choice to model 'int' as 8 bytes, or on mc's
# specific stack-frame layout — carry a "// no-diff" directive in
# their first 5 lines and are skipped.
#
# Like test/test.sh, this script only runs end-to-end on Linux,
# because mc emits x86-64 System V assembly.

set -u
pass=0
fail=0
skip=0
failed=()

MC=./mc
[ -x "$MC" ] || MC=./mc.exe

for src in test/case_*.c; do
  [ -e "$src" ] || continue

  if head -5 "$src" | grep -q '^// no-diff'; then
    skip=$((skip+1))
    continue
  fi

  if ! "$MC" "$src" > tmp_mc.s 2>err; then
    echo "FAIL mc compile: $src"
    sed 's/^/    /' err
    fail=$((fail+1)); failed+=("$src"); continue
  fi
  if ! gcc tmp_mc.s -o tmp_mc.out 2>err; then
    echo "FAIL mc link: $src"
    sed 's/^/    /' err
    fail=$((fail+1)); failed+=("$src"); continue
  fi
  if ! gcc -O0 -w "$src" -o tmp_gcc.out 2>err; then
    # gcc rejected the source — could be syntax mc accepts that gcc
    # doesn't, or vice versa. Skip with a note rather than fail.
    echo "SKIP (gcc rejected): $src"
    skip=$((skip+1))
    continue
  fi

  out_mc=$(./tmp_mc.out 2>/dev/null);  rc_mc=$?
  out_gcc=$(./tmp_gcc.out 2>/dev/null); rc_gcc=$?

  if [ "$rc_mc" = "$rc_gcc" ] && [ "$out_mc" = "$out_gcc" ]; then
    pass=$((pass+1))
  else
    echo "DIFF $src: mc=(rc=$rc_mc, '$out_mc')  gcc=(rc=$rc_gcc, '$out_gcc')"
    fail=$((fail+1)); failed+=("$src")
  fi
done

rm -f tmp_mc.s tmp_mc.out tmp_gcc.out err
echo "diff: $pass passed, $fail failed, $skip skipped"
[ "$fail" -eq 0 ]
