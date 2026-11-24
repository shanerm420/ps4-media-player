#!/usr/bin/env python3
"""check-links.py - fail if any relative link or heading anchor is broken.

Runs on the published tree, so it needs nothing but this file and the pages.
CI runs it on every push; publish-docs.py runs the same rules before publishing.

    python3 tools/check-links.py
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def slug(text):
    s = re.sub(r"[`*_~]", "", text.strip().lower())
    s = re.sub(r"[^\w\s-]", "", s)
    return re.sub(r"\s+", "-", s)


def anchors(path):
    text = open(path, encoding="utf-8").read()
    return {slug(t) for t in re.findall(r"^#{1,6} (.+)$", text, re.M)} | {"top"}


# `_scratch/` holds throwaway push clones: a partial copy of the published tree,
# so the links inside it resolve against files that were never copied. It is not
# published and must not fail this check, the same way rule-index.py skips it.
pages = [os.path.join(b, n) for b, _d, ns in os.walk(ROOT)
         if os.sep + ".git" not in b and os.sep + "_scratch" not in b
         for n in ns if n.endswith((".md", ".html"))]
bad = []
for page in pages:
    text = open(page, encoding="utf-8").read()
    for m in re.finditer(r"\]\(([^)\s]+)\)", text):
        t = m.group(1)
        if t.startswith(("http://", "https://", "mailto:")):
            continue
        path, _, frag = t.partition("#")
        dest = os.path.normpath(os.path.join(os.path.dirname(page),
                                             path or os.path.basename(page)))
        if not (path == "" or os.path.exists(dest)):
            bad.append("%s -> %s" % (os.path.relpath(page, ROOT), t))
        elif frag and dest.endswith((".md", ".html")) and frag not in anchors(dest):
            bad.append("%s -> %s (no such anchor)" % (os.path.relpath(page, ROOT), t))

for b in bad:
    print("FAIL " + b)
print("check-links: %d pages, %d problems" % (len(pages), len(bad)))
sys.exit(1 if bad else 0)
