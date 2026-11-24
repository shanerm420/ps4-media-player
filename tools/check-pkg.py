#!/usr/bin/env python3
"""Pre-flight a PS4 package before it is handed to the console.

Why this exists (2026-11-24): GPU-Bloom-Slope-Probe-BREW00102 installed as
"Corrupted Data" (CE-33481-9) and then refused to start (CE-34544-0). The
package's own container was fine - its `sce_sys/param.sfo` had 7 entries
instead of 11, because a build was interrupted while the SFO was being written
and every later `make` saw the half-written file as newer than the Makefile and
kept it. The console cannot name the app without TITLE and TITLE_ID, so it
rejects the lot. Nothing in the build noticed.

Usage:  python3 tools/check-pkg.py <project dir>
Exit:   0 all checks pass, 1 one or more fail (the package must not be shipped).
"""

import os
import re
import struct
import sys

REQUIRED = [
    "APP_TYPE", "APP_VER", "ATTRIBUTE", "CATEGORY", "CONTENT_ID", "FORMAT",
    "DOWNLOAD_DATA_SIZE", "SYSTEM_VER", "TITLE", "TITLE_ID", "VERSION",
]

fails = []
passes = []


def check(name, ok, detail=""):
    (passes if ok else fails).append(name)
    print("%s %s%s" % ("PASS" if ok else "FAIL", name,
                       ("  (" + detail + ")") if detail else ""))


def read_sfo(path):
    """Return (entries dict, count) for a PSF/SFO file, or raise."""
    d = open(path, "rb").read()
    if len(d) < 20 or d[:4] != b"\x00PSF":
        raise ValueError("not a param.sfo (bad magic)")
    _, _, key_start, data_start, count = struct.unpack("<4sIIII", d[:20])
    out = {}
    for i in range(count):
        off = 20 + i * 16
        if off + 16 > len(d):
            raise ValueError("entry %d runs past the end of the file" % i)
        ko, fmt, length, _max, doff = struct.unpack("<HHIII", d[off:off + 16])
        start = key_start + ko
        end = d.find(b"\x00", start)
        if end < 0:
            raise ValueError("entry %d has no key terminator" % i)
        key = d[start:end].decode("latin1")
        value = d[data_start + doff:data_start + doff + length]
        if fmt in (4, 0x0204):            # utf8 / utf8-s
            value = value.split(b"\x00")[0].decode("latin1")
        elif isinstance(value, bytes):
            value = value.rstrip(b"\x00")
        out[key] = value
    return out, count


def main():
    if len(sys.argv) != 2:
        print("usage: check-pkg.py <project dir>")
        return 2

    proj = sys.argv[1].rstrip("/")
    mk = os.path.join(proj, "Makefile")
    if not os.path.isfile(mk):
        print("FAIL no Makefile in %s" % proj)
        return 1

    text = open(mk).read()
    def var(name):
        m = re.search(r"^%s\s*:=\s*(\S+)" % name, text, re.M)
        return m.group(1) if m else None

    title_id = var("TITLE_ID")
    content_id = var("CONTENT_ID")
    check("Makefile states TITLE_ID and CONTENT_ID", bool(title_id and content_id),
          "%s / %s" % (title_id, content_id))

    if content_id:
        check("content id is exactly 36 characters", len(content_id) == 36,
              "%d" % len(content_id))
        check("content id carries this title id", content_id[7:16] == title_id,
              content_id[7:16])

    sfo_path = os.path.join(proj, "sce_sys", "param.sfo")
    if not os.path.isfile(sfo_path):
        print("FAIL sce_sys/param.sfo is missing")
        return 1

    try:
        entries, count = read_sfo(sfo_path)
    except ValueError as exc:
        print("FAIL param.sfo cannot be read: %s" % exc)
        return 1

    missing = [k for k in REQUIRED if k not in entries]
    check("param.sfo has every required entry", not missing,
          "missing " + ", ".join(missing) if missing else "%d entries" % count)
    check("param.sfo entry count is 11", count == 11, "%d" % count)

    if content_id and "CONTENT_ID" in entries:
        check("param.sfo CONTENT_ID matches the Makefile",
              entries["CONTENT_ID"] == content_id, str(entries.get("CONTENT_ID")))
    if title_id and "TITLE_ID" in entries:
        check("param.sfo TITLE_ID matches the Makefile",
              entries["TITLE_ID"] == title_id, str(entries.get("TITLE_ID")))
    if "TITLE" in entries:
        check("param.sfo TITLE is set", bool(entries["TITLE"]),
              repr(entries.get("TITLE")))

    pkg = os.path.join(proj, (content_id or "x") + ".pkg")
    if not os.path.isfile(pkg):
        print("FAIL %s is missing - build it before shipping" % os.path.basename(pkg))
        return 1
    with open(pkg, "rb") as fh:
        head = fh.read(4)
    size = os.path.getsize(pkg)
    check("package starts with the CNT container magic", head == b"\x7fCNT", head.hex())
    check("package is not truncated at a suspicious size", size > 1 << 20,
          "%d bytes" % size)

    print("\npackage check: %d passed, %d failed" % (len(passes), len(fails)))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
