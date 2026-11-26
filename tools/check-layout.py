#!/usr/bin/env python3
"""check-layout.py - prove a built package's header describes the file it is.

The installer trusts three numbers absolutely: where the body ends, where the
PFS image starts, and how big the whole package claims to be. When they
disagree the package either refuses to install or installs half of itself, and
the error you get back on the console names none of them. So they are checked
here, at build time, on every package we produce.

What is asserted, and what is deliberately not:

  * entry_count == entry_count_2, and the entry table fits inside the body.
    Safe unconditionally - every implementation sizes the table from
    entry_count (entry_count_2 is the same count in 16 bits).
  * body_offset + body_size == pfs_image_offset, and package_size == file size.
    Only when the package actually carries a PFS image. A PS4AL stub
    (content_type 0x1C) and a delta patch legitimately leave pfs_image_offset
    and package_size at 0 while still being valid files, and a split piece of a
    >4 GiB package carries only part of the whole. A gate that failed on those
    would be worse than no gate.
  * promote_size is NOT asserted to be zero and mount_image_offset is NOT
    asserted to equal pfs_image_offset: every GD package sets promote_size to
    the pre-PFS size, and mount_image_size is the whole package, not the image.

Usage: check-layout.py [project dir]   (defaults to the current directory)
"""
import glob
import os
import struct
import sys

# Offsets as observed in a real package (our v144) and in the frozen early block
# of a retail dump. The published struct puts body geometry at 0x400 and the
# image block at 0x47C; that does not match a file, so these were taken from the
# bytes and required to reproduce v144's known values: body 0x2000 + 0x7E000,
# image 0x80000, image 6094848 bytes, package 6619136 bytes = the file size.
OFF_PKG_TYPE = 0x04         # u32  1 = GD, 0x1C = AL ("no data")
OFF_ENTRY_COUNT = 0x10      # u32  pkg_entry_count
OFF_SC_COUNT = 0x14         # u16  pkg_sc_entry_count
OFF_ENTRY_COUNT_2 = 0x16    # u16  same count as 0x10
OFF_TABLE = 0x18            # u32  entry_table_offset
OFF_BODY_OFF = 0x20         # u64  body_offset
OFF_BODY_SIZE = 0x28        # u64  body_size
OFF_CONTENT_ID = 0x40       # char[0x24]
OFF_PFS_OFF = 0x410         # u64  pfs_image_offset
OFF_PFS_SIZE = 0x418        # u64  pfs_image_size
OFF_PKG_SIZE = 0x428        # u64  package_size
ENTRY_SIZE = 0x20


def u32(b, off):
    return struct.unpack_from(">I", b, off)[0]


def u64(b, off):
    return struct.unpack_from(">Q", b, off)[0]


def cstr(b, off, n):
    return b[off:off + n].split(b"\0")[0].decode("ascii", "replace")


def main():
    proj = (sys.argv[1] if len(sys.argv) > 1 else ".").rstrip("/") or "."
    pkgs = sorted(glob.glob(os.path.join(proj, "*.pkg")),
                  key=os.path.getmtime, reverse=True)
    if not pkgs:
        print("check-layout: no .pkg in %s (nothing built yet) - skipped" % proj)
        return 0
    path = pkgs[0]
    size = os.path.getsize(path)
    fails = 0

    def check(name, ok, detail=""):
        nonlocal fails
        print("%s %s%s" % ("PASS" if ok else "FAIL", name, (" - " + detail) if detail else ""))
        if not ok:
            fails += 1

    with open(path, "rb") as f:
        h = f.read(0x800)

    check("package magic is CNT", h[:4] == b"\x7FCNT", h[:4].hex())

    entries = u32(h, OFF_ENTRY_COUNT)
    entries2 = struct.unpack_from(">H", h, OFF_ENTRY_COUNT_2)[0]
    sc = struct.unpack_from(">H", h, OFF_SC_COUNT)[0]
    table = u32(h, OFF_TABLE)
    pkg_type = u32(h, OFF_PKG_TYPE)
    body_off = u64(h, OFF_BODY_OFF)
    body_size = u64(h, OFF_BODY_SIZE)
    pfs_off = u64(h, OFF_PFS_OFF)
    pfs_size = u64(h, OFF_PFS_SIZE)
    pkg_size = u64(h, OFF_PKG_SIZE)
    cid = cstr(h, OFF_CONTENT_ID, 0x24)

    check("entry_count_2 equals entry_count", entries == entries2,
          "%d vs %d" % (entries, entries2))
    check("entry table fits inside the body region",
          body_off <= table and table + entries * ENTRY_SIZE <= body_off + body_size,
          "table 0x%X + %d rows ends 0x%X, body 0x%X..0x%X"
          % (table, entries, table + entries * ENTRY_SIZE, body_off, body_off + body_size))
    check("content id is 36 characters", len(cid) == 36, cid)

    check("package is a game package or a no-data stub",
          pkg_type in (1, 0x1C), "type 0x%X" % pkg_type)
    has_pfs = pfs_size != 0 and pkg_type != 0x1C
    if has_pfs:
        check("body_off + body_size == pfs_image_offset",
              body_off + body_size == pfs_off,
              "0x%X + 0x%X = 0x%X, image at 0x%X"
              % (body_off, body_size, body_off + body_size, pfs_off))
        check("package_size equals the file size", pkg_size == size,
              "%d vs %d" % (pkg_size, size))
        check("pfs image ends at or before the file end",
              pfs_off + pfs_size <= size,
              "0x%X + %d = %d, file %d" % (pfs_off, pfs_size, pfs_off + pfs_size, size))
    else:
        print("SKIP  layout equalities - no PFS image (type 0x%X), which is legal" % pkg_type)

    print("check-layout: %s, type 0x%X, %d entries + %d sc rows, %d bytes"
          % (os.path.basename(path), pkg_type, entries, sc, size))
    print("check-layout: %d failed" % fails)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
