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

import hashlib
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import buildstamp
except ImportError:  # the tool must not be fatal if it is missing
    buildstamp = None

# The keys the console needs to name and run the app. FORMAT used to be in this
# list and is not written by the Makefile; the check that carried it had never
# been executed, so it was asking for an entry the shipped packages never had
# (found 2026-11-25, the first time this gate actually ran). The full expected
# set is now read from the Makefile itself, so the two cannot drift.
REQUIRED = ["APP_VER", "CATEGORY", "CONTENT_ID", "TITLE", "TITLE_ID", "VERSION"]

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


def _newest_source(proj):
    """Newest mtime among the files that go into the binary."""
    newest = 0.0
    for root in ("src", "third_party"):
        for base, _dirs, names in os.walk(os.path.join(proj, root)):
            for n in names:
                if n.endswith((".c", ".h")):
                    newest = max(newest, os.path.getmtime(os.path.join(base, n)))
    mk = os.path.join(proj, "Makefile")
    if os.path.isfile(mk):
        newest = max(newest, os.path.getmtime(mk))
    return newest


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
    version = var("VERSION")
    pkgver = var("PKGVER")
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

    # The expected set comes from the recipe, not from memory: every key the
    # Makefile sets must be in the file, and nothing may be extra or missing.
    written = re.findall(r"sfo_setentry\s+\S+\s+([A-Z][A-Z0-9_]*)", text)
    missing = [k for k in REQUIRED if k not in entries]
    check("param.sfo has the keys the console needs", not missing,
          "missing " + ", ".join(missing) if missing else "all present")
    check("param.sfo has exactly the keys the Makefile writes",
          sorted(entries) == sorted(written),
          "file %d, Makefile %d%s" % (len(entries), len(written),
          "; extra " + ", ".join(sorted(set(entries) - set(written))) if set(entries) - set(written)
          else "; missing " + ", ".join(sorted(set(written) - set(entries)))))

    if content_id and "CONTENT_ID" in entries:
        check("param.sfo CONTENT_ID matches the Makefile",
              entries["CONTENT_ID"] == content_id, str(entries.get("CONTENT_ID")))
    if title_id and "TITLE_ID" in entries:
        check("param.sfo TITLE_ID matches the Makefile",
              entries["TITLE_ID"] == title_id, str(entries.get("TITLE_ID")))
    if "TITLE" in entries:
        check("param.sfo TITLE is set", bool(entries["TITLE"]),
              repr(entries.get("TITLE")))

    # The console shows APP_VER, and the installer compares versions. A package
    # that still carries the previous number installs as a duplicate at best
    # (found 2026-11-25: VERSION 1.35, package APP_VER 1.34, because param.sfo
    # had no prerequisite).
    if version:
        check("param.sfo APP_VER matches the Makefile VERSION",
              entries.get("APP_VER") == version, str(entries.get("APP_VER")))
        check("param.sfo VERSION matches the Makefile VERSION",
              entries.get("VERSION") == version, str(entries.get("VERSION")))
        if pkgver:
            check("PKGVER is VERSION without the dot", pkgver == version.replace(".", ""),
                  "%s vs %s" % (pkgver, version))

    icon = os.path.join(proj, "sce_sys", "icon0.png")
    if os.path.isfile(icon):
        d = open(icon, "rb").read(33)
        ok = d[:8] == b"\x89PNG\r\n\x1a\n" and d[12:16] == b"IHDR"
        w = struct.unpack(">I", d[16:20])[0] if ok else 0
        h = struct.unpack(">I", d[20:24])[0] if ok else 0
        check("icon0.png is a 512x512 PNG", ok and w == 512 and h == 512,
              "%dx%d" % (w, h))
    else:
        check("icon0.png exists (the XMB tile)", False, "sce_sys/icon0.png missing")

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

    # Does this package come from this source? The package is not reproducible
    # (create-gp4 and PkgTool stamp the clock), so an MD5 cannot answer that.
    # The source hash compiled into the log line can, and it is readable from
    # the shipped eboot.bin - the console log then names the build it ran.
    eboot = os.path.join(proj, "eboot.bin")
    if buildstamp and os.path.isfile(eboot):
        src = buildstamp.source_id(proj)
        blob = open(eboot, "rb").read()
        check("eboot.bin carries this source's hash", src.encode() in blob, src)
        check("the build id names version and source",
              ("v" + (version or "") + "-" + src).encode() in blob,
              "v%s-%s" % (version, src))
        check("eboot.bin is not older than the source it claims",
              os.path.getmtime(eboot) >= _newest_source(proj),
              "eboot mtime vs newest source file")
    else:
        check("eboot.bin present for the source check", False, "eboot.bin missing")

    print("\npackage check: %d passed, %d failed" % (len(passes), len(fails)))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
