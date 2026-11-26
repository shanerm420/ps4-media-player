#!/usr/bin/env python3
"""buildstamp.py - print the -D flags that identify a build.

Two different jobs used to be confused here.

**A stable clock.** `eboot.bin` embeds `__DATE__` and `__TIME__`, so two builds
of identical source used to differ for no reason at all. The compiler now
receives a fixed stamp instead, so a rebuild of unchanged source is a fair
comparison.

    make                      -> the constant below
    SOURCE_DATE_EPOCH=0 make  -> 1970-01-01 00:00:00

**An identity.** The clock is not an identity, and this script used to claim the
package was reproducible: it is not. Measured 2026-11-25 - the same source, the
same `eboot.bin`, two packages, different MD5s - because `create-gp4` writes
`volume_ts` from the clock and PkgTool writes `PUBTOOLINFO c_date` from the
clock. So an MD5 names a build, never a source state.

What actually ties a package to its source is a hash of the source, compiled
into the binary as `BUILD_SRC` and printed in the boot line. `tools/check-pkg.py`
extracts it from the shipped `eboot.bin` and refuses a package whose hash does
not match the tree it is being shipped from.

Read-only, no state. Consumed by the Makefile as CFLAGS.
"""

import hashlib
import os
import sys
import time

# Directories whose contents define "the source". Anything not listed here can
# change without changing the identity of the binary.
ROOTS = ("src", "third_party")
EXTRA = ("Makefile",)
SUFFIX = (".c", ".h")


def source_id(proj):
    """First 12 hex digits of a sha256 over the source tree, names included."""
    h = hashlib.sha256()
    files = []
    for root in ROOTS:
        for base, dirs, names in os.walk(os.path.join(proj, root)):
            dirs.sort()
            for n in sorted(names):
                if n.endswith(SUFFIX):
                    files.append(os.path.join(base, n))
    for n in EXTRA:
        p = os.path.join(proj, n)
        if os.path.isfile(p):
            files.append(p)
    for p in sorted(files):
        rel = os.path.relpath(p, proj).replace(os.sep, "/")
        h.update(rel.encode("utf-8") + b"\x00")
        with open(p, "rb") as f:
            h.update(f.read())
        h.update(b"\x00")
    return h.hexdigest()[:12]


def main(argv):
    proj = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    version = argv[1] if len(argv) > 1 else "0.00"

    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    if epoch and epoch.isdigit():
        d = time.strftime("%Y-%m-%d", time.gmtime(int(epoch)))
        t = time.strftime("%H:%M:%S", time.gmtime(int(epoch)))
    else:
        d, t = "2026-11-24", "00:00:00"

    # Single quotes outside, double quotes inside: make hands this line to the
    # shell, which strips the outer single quotes and leaves the compiler a
    # string literal. Printing plain "double quotes" here loses them to the
    # shell, so BUILD_TIME becomes the token sequence 00 : 00 : 00 (a compile
    # error) and BUILD_DATE silently becomes the integer 2026-11-24.
    print("-DBUILD_DATE='\"%s\"' -DBUILD_TIME='\"%s\"' "
          "-DBUILD_VER='\"%s\"' -DBUILD_SRC='\"%s\"'"
          % (d, t, version, source_id(proj)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
