#!/usr/bin/env python3
"""buildstamp.py - print the -D flags that make a package reproducible.

`eboot.bin` embeds `__DATE__` and `__TIME__`, so two builds of identical source
used to produce different packages and `cmp` could never prove anything. The
compiler now receives a fixed stamp instead:

    make                      -> the constant below (a build is a copy of a source state)
    SOURCE_DATE_EPOCH=0 make  -> 1970-01-01 00:00:00

Read-only, no state. Consumed by the Makefile as CFLAGS.
"""

import os
import time

DEFAULT = ("2026-11-24", "00:00:00")
epoch = os.environ.get("SOURCE_DATE_EPOCH")

if epoch and epoch.isdigit():
    d = time.strftime("%Y-%m-%d", time.gmtime(int(epoch)))
    t = time.strftime("%H:%M:%S", time.gmtime(int(epoch)))
else:
    d, t = DEFAULT

print("-DBUILD_DATE='\"%s\"' -DBUILD_TIME='\"%s\"'" % (d, t))
