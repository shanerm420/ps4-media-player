#!/bin/sh
# stamp_test.sh - the build stamp must reach the compiler as a string literal.
#
# Why this exists: tools/buildstamp.py printed its -D flags with plain double
# quotes, make handed the recipe line to the shell, the shell ate the quotes, and
# the compiler received -DBUILD_TIME=00:00:00. That is a syntax error at the use
# site in src/main.c, and BUILD_DATE also collapsed to the integer 1991. Nothing
# in `make check` compiled anything, so a tree that could not produce a package
# passed every gate for a whole round. This test compiles six lines of C with
# exactly the flags the Makefile would pass, through a shell, and fails loudly.
#
# Runs on the host: no PS4 toolchain needed, which is why it can run in CI.

set -u
cd "$(dirname "$0")/.." || exit 1

CC=${CC:-cc}
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

if ! flags=$(python3 tools/buildstamp.py); then
    echo "FAIL  tools/buildstamp.py did not run"
    exit 1
fi

cat > "$tmp/t.c" <<'EOF'
#include <stdio.h>
int main(void) { printf("%s %s\n", BUILD_DATE, BUILD_TIME); return 0; }
EOF

# The one line that matters: build the command as text and let a shell parse it,
# which is what make does with a recipe line. Quoting it here would hide the bug.
if ! sh -c "$CC $flags -o '$tmp/t' '$tmp/t.c'" 2>"$tmp/err"; then
    echo "FAIL  the stamp flags do not compile through a shell"
    echo "      flags were: $flags"
    sed 's/^/      /' "$tmp/err"
    exit 1
fi

out=$("$tmp/t")
case $out in
    19[0-9][0-9]-*|20[0-9][0-9]-*)
        echo "PASS  build stamp reaches the compiler as a string literal ($out)"
        ;;
    *)
        echo "FAIL  the build stamp printed '$out', which is not a date string"
        echo "      a bare -DBUILD_DATE=2026-11-24 becomes arithmetic, i.e. 1991"
        exit 1
        ;;
esac
