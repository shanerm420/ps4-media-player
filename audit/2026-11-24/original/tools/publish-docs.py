#!/usr/bin/env python3
"""publish-docs.py - build the linked publishing tree from the canonical docs.

The canonical documents stay where they are: `MANIFEST.md`, `AGENTS.md`,
`HISTORY.md`, `PLAN.md` next to this tool. This script publishes them to GitHub
as a linked system, in dependency order, so every page exists before the page
that links to it:

    leaves      docs/rules/*.md, docs/amendments/*.md, docs/HISTORY.md
    middle      docs/manifest/01..12-*.md, docs/rules/README.md
    top         docs/README.md, AGENTS.md, STATUS.html, RULES.index, tools/

Nothing here is hand-edited after the fact: the tree is regenerated from the
canonical files, and every relative link is resolved before the script exits.
A dangling link or a missing anchor fails the run, which is what lets CI prove
the published tree is as consistent as the working one.

    python3 tools/publish-docs.py            # build _scratch/publish/
    python3 tools/publish-docs.py --check    # build, then only report links
"""

import hashlib
import json
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = os.path.dirname(ROOT)
OUT = os.path.join(ROOT, "_scratch", "publish")
TOOLS = ("check-manifest.py", "rule-index.py", "status.py", "buildstamp.py",
         "check-links.py")


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def slug(text):
    """GitHub's heading anchor: lowercase, punctuation dropped, spaces to '-'."""
    s = text.strip().lower()
    s = re.sub(r"[`*_~]", "", s)
    s = re.sub(r"[^\w\s-]", "", s)
    return re.sub(r"\s+", "-", s)


def write(rel, text):
    p = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        f.write(text)


def sec_name(num, title):
    return "%02d-%s.md" % (int(num), slug(title))


def main():
    man = read(os.path.join(ROOT, "MANIFEST.md"))
    agents = read(os.path.join(ROOT, "AGENTS.md"))
    hist = read(os.path.join(FILES, "HISTORY.md"))
    plan = read(os.path.join(FILES, "PLAN.md"))
    index = json.load(open(os.path.join(FILES, "RULES.index"), encoding="utf-8"))

    heads = [(int(m.group(1)), m.group(2), m.start())
             for m in re.finditer(r"^## (\d+)\. (.+)$", man, re.M)]
    sections = []
    for i, (num, title, at) in enumerate(heads):
        end = heads[i + 1][2] if i + 1 < len(heads) else len(man)
        body = man[at:end]
        body = body.split("\n", 1)[1] if "\n" in body else ""
        sections.append((num, title, body.strip()))

    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)

    # ---------------------------------------------------------------- leaves
    # Rule pages first: nothing links to them until the indexes are written.
    gtitle = {("G" + m.group(1)): m.group(2).rstrip(".")
              for m in re.finditer(r"^\*\*G(\d+) — (.+?)\*\*", agents, re.M)}
    table = {}
    for m in re.finditer(r"^\| (R\d+) \| (.+?) \| (.+?) \| (.+?) \|\s*$", man, re.M):
        table[m.group(1)] = (m.group(2).strip(), m.group(3).strip(), m.group(4).strip())

    rule_pages = {}
    for r in index["rules"]:
        rid, kind = r["id"], r["kind"]
        if kind == "R":
            statement, status, evidence = table.get(rid, (r["statement"], r["status"],
                                                          r["evidence"]))
            body = ["# %s" % rid, "", statement, "",
                    "**Status:** %s  " % status.replace("**", ""),
                    "**Evidence:** %s" % evidence,
                    "",
                    "Full row: [manifest §9](../manifest/09-ps4-knowledge-base.md). ",
                    "Part of the [rule index](README.md)."]
        else:
            full = gtitle.get(rid, r["statement"])
            _st = r["statement"]
            _line = ("**%s.**" % full.rstrip(".")
                     if _st.strip(".").lower()[:25] == full.strip(".").lower()[:25]
                     else "**%s.** %s" % (full.rstrip("."), _st))
            body = ["# %s" % rid, "", _line,
                    "",
                    "**Status:** %s  " % r["status"],
                    "**Evidence:** %s" % (r["evidence"] or "see the rule text"),
                    "",
                    "Full text: [%s in AGENTS.md](../../AGENTS.md#%s)"
                    % (rid, slug("%s — %s" % (rid, full))),
                    "",
                    "Part of the [rule index](README.md)."]
        write("docs/rules/%s.md" % rid, "\n".join(body) + "\n")
        rule_pages[rid] = "docs/rules/%s.md" % rid

    amendments = re.findall(r"^\*\*A(\d+) — (.+?)\*\*(.*?)(?=^\*\*A\d+ —|^## )",
                            agents, re.S | re.M)
    for num, title, body in amendments:
        write("docs/amendments/A%s.md" % num,
              "# A%s — %s\n\n%s\n\nBack to [the rule index](../rules/README.md).\n"
              % (num, title.strip(), body.strip()))
    write("docs/amendments/README.md",
          "# Amendments\n\nA rule that later evidence changed. The rule now reads as\n"
          "amended; the wording before is kept so a reader can see what moved and why.\n\n"
          + "".join("- [A%s — %s](A%s.md)\n" % (n, t.strip(), n)
                    for n, t, _b in amendments)
          + "\n[Back to the rule index](../rules/README.md).\n")

    write("docs/HISTORY.md",
          "# Build history\n\nThe dated log: what was built, what ran on the console,\n"
          "what the log said. Read it when you need to know *why* a rule exists.\n\n"
          "[Back to the index](README.md).\n\n---\n\n" + hist)

    # ---------------------------------------------------------------- middle
    write("docs/rules/README.md",
          "# Rule index\n\nEvery rule in the project, with its status and the evidence\n"
          "behind it. Generated from `MANIFEST.md` §9 and `AGENTS.md` by\n"
          "`tools/rule-index.py`; the machine-readable form is `RULES.index`.\n\n"
          "Status vocabulary: **measured** (a console log proves it), **researched only**\n"
          "(a source says so), **assumed** (a starting point, not a finding),\n"
          "**contested** (two readings, a test settles it), **procedural** (a method).\n\n"
          "## Rules that shape the work (G)\n\n"
          + "".join("- [%s](%s.md) — %s `%s`\n"
                    % (r["id"], r["id"], r["statement"], r["status"])
                    for r in index["rules"] if r["kind"] == "G")
          + "\n## Graphics and hardware facts (R)\n\n"
          + "".join("- [%s](%s.md) — %s `%s`\n"
                    % (r["id"], r["id"], r["statement"], r["status"])
                    for r in index["rules"] if r["kind"] == "R")
          + "\n## Amendments\n\nSee [amendments](../amendments/README.md).\n\n"
          "[Back to the index](../README.md).\n")

    for i, (num, title, body) in enumerate(sections):
        prev_p = ("[%s](%s)" % (sections[i - 1][1], sec_name(*sections[i - 1][:2]))
                  if i else "—")
        next_p = ("[%s](%s)" % (sections[i + 1][1], sec_name(*sections[i + 1][:2]))
                  if i + 1 < len(sections) else "—")
        body = re.sub(r"§(\d+)", lambda m: "[§%s](%s)" % (
            m.group(1),
            sec_name(*next((s[:2] for s in sections if s[0] == int(m.group(1))),
                           (m.group(1), "missing")))), body)
        write("docs/manifest/" + sec_name(num, title),
              "# %d. %s\n\n*Part %d of 12 of the manifest — previous: %s · next: %s*\n\n%s\n\n"
              "[Manifest index](README.md) · [Back to the index](../README.md)\n"
              % (num, title, num, prev_p, next_p, body))

    write("docs/manifest/README.md",
          "# Manifest\n\nThe spine of the project, in twelve parts. Read §2 for where we\n"
          "are, §9 for what we know, and §11 for what happens next.\n\n"
          + "".join("%d. [%s](%s)\n" % (n, t, sec_name(n, t)) for n, t, _b in sections)
          + "\n[Back to the index](../README.md)\n")

    # ------------------------------------------------------------------- top
    write("docs/README.md",
          "# PS4 media player — project knowledge\n\n"
          "A native PS4 home media player built with the OpenOrbis toolchain, and the\n"
          "documented system that keeps its engineering honest: rules with statuses,\n"
          "measurements with provenance, a dated log, and checkers that fail when the\n"
          "paperwork stops being true.\n\n"
          "## If you are an AI agent, read in this order\n\n"
          "1. [AGENTS.md](AGENTS.md) — the rules. Hard rules, workspace rules,\n"
          "   measurement rules, and G1–G10.\n"
          "2. [Manifest §2](docs/manifest/02-current-focus.md) — where the project is\n"
          "   right now: latest builds, the blocking question, the next action.\n"
          "3. [Rule index](docs/rules/README.md) — every rule with its status and the\n"
          "   evidence behind it. `measured` means a console log proves it.\n"
          "4. [Manifest §9](docs/manifest/09-ps4-knowledge-base.md) — the knowledge\n"
          "   base: what this console does, measured on this console.\n"
          "5. [History](docs/HISTORY.md) — why each rule exists.\n\n"
          "Nothing later in that order overrides anything earlier.\n\n"
          "## What is here\n\n"
          "- [Manifest](docs/manifest/README.md) — 12 parts, from the goal to the\n"
          "  merged PS4 development standard\n"
          "- [Rules](docs/rules/README.md) — every rule, each on its own page\n"
          "- [Amendments](docs/amendments/README.md) — rules that later evidence changed\n"
          "- [History](docs/HISTORY.md) — the dated build log\n"
          "- [Status board](docs/STATUS.html) — generated: every check, the current\n"
          "  state, what is still waiting on the console\n"
          "- [RULES.index](docs/RULES.index) — the same rules, machine-readable\n"
          "- [Piglet notes](../PIGLET-NOTES.md) — what other PS4 homebrew already\n"
          "  solved, and what is still ours to measure\n"
          "- [Round notes](../ROUND-NOTES.md) — the round-by-round history (appendix)\n"
          "- [Logging appendix](../APPENDIX-LOGS.md) — the log survey and dead ends\n"
          "- [tools](tools/) — the checkers that keep all of the above consistent\n\n"
          "## Honesty note\n\n"
          "Every claim here carries a status. `measured` means a log from this console\n"
          "says so; `assumed` means nobody has checked yet. Numbers without a status are\n"
          "a bug, and the checkers fail on them.\n\n"
          "Source code lives in the project repository alongside this documentation;\n"
          "packages are kept out of git history deliberately.\n")

    _idx = read(os.path.join(OUT, "docs/README.md"))
    for _a, _b in (("](docs/manifest/", "](manifest/"),
                   ("](docs/rules/", "](rules/"),
                   ("](docs/amendments/", "](amendments/"),
                   ("](docs/HISTORY.md)", "](HISTORY.md)"),
                   ("](docs/STATUS.html)", "](STATUS.html)"),
                   ("](docs/RULES.index)", "](RULES.index)"),
                   ("](docs/README.md)", "](README.md)"),
                   ("](AGENTS.md)", "](../AGENTS.md)"),
                   ("](tools/)", "](../tools/)")):
        _idx = _idx.replace(_a, _b)
    write("docs/README.md", _idx)

    # No root README is published: the repository already has one, and a second
    # copy would either overwrite it or break its links. docs/README.md is the
    # index, and it is linked from AGENTS.md.

    top = ("# Working rules — PS4 media player\n\n"
           "**Read order:** [docs/README.md](docs/README.md) → this file (the rules) → "
           "[manifest §2](docs/manifest/02-current-focus.md) → "
           "[rule index](docs/rules/README.md) → "
           "[manifest §9](docs/manifest/09-ps4-knowledge-base.md) → "
           "[history](docs/HISTORY.md).\n\n"
           "This is the published copy of the rules file. It is regenerated from the\n"
           "project's canonical `AGENTS.md` by `tools/publish-docs.py`; §-references\n"
           "point into the published manifest.\n\n---\n\n")
    agents_body = re.sub(r"^\*\*G(\d+) — (.+?)\*\*(.*)$",
                         lambda m: "### G%s — %s\n\n%s" % (
                             m.group(1), m.group(2).rstrip("."), m.group(3).strip()),
                         agents, flags=re.M)
    agents_body = re.sub(r"§(\d+)", lambda m: "[§%s](docs/manifest/%s)" % (
        m.group(1), sec_name(*next((s[:2] for s in sections if s[0] == int(m.group(1))),
                                  (m.group(1), "missing")))), agents_body)
    write("AGENTS.md", top + agents_body)

    write("docs/PLAN.md",
          "# Plan\n\nWhat happens next, as a work plan — not a rule. Nothing in it\n"
          "overrides `AGENTS.md`.\n\n[Back to the index](README.md).\n\n---\n\n" + plan)

    # Notes and appendices the rules point at by name. Published at the root,
    # next to AGENTS.md, so a name written in a rule is a real page on GitHub.
    for _name in ("PIGLET-NOTES.md", "ROUND-NOTES.md", "APPENDIX-LOGS.md"):
        _src = os.path.join(ROOT, _name)
        if os.path.exists(_src):
            write(_name, read(_src))

    write(".github/workflows/docs.yml", WORKFLOW)

    for name, rel in (("STATUS.html", "docs/STATUS.html"),
                      ("RULES.index", "docs/RULES.index")):
        src = os.path.join(FILES, name)
        if os.path.exists(src):
            write(rel, read(src))
    os.makedirs(os.path.join(OUT, "tools"), exist_ok=True)
    for name in TOOLS:
        src = os.path.join(ROOT, "tools", name)
        if os.path.exists(src):
            shutil.copy2(src, os.path.join(OUT, "tools", name))

    sums = []
    for b, _d, ns in os.walk(OUT):
        for n in sorted(ns):
            fp = os.path.join(b, n)
            sums.append("%s  %s" % (hashlib.md5(open(fp, "rb").read()).hexdigest(),
                                    os.path.relpath(fp, OUT)))
    write("CHECKSUMS.txt",
          "Every file in this tree, with its MD5. The published copy must match\n"
          "these hashes byte for byte; they are not to be regenerated by hand.\n\n"
          + "\n".join(sorted(sums, key=lambda x: x.split("  ")[1])) + "\n")

    # ------------------------------------------------------------ link check
    pages, problems = [], []
    for base, _dirs, names in os.walk(OUT):
        for n in names:
            if n.endswith((".md", ".html")):
                pages.append(os.path.join(base, n))

    def anchors(path):
        if not path.endswith((".md", ".html")):
            return set()
        text = read(path)
        return {slug(t) for t in re.findall(r"^#{1,6} (.+)$", text, re.M)} | {"top"}

    for page in pages:
        text = read(page)
        for m in re.finditer(r"\]\(([^)\s]+)\)", text):
            target = m.group(1)
            if target.startswith(("http://", "https://", "mailto:")):
                continue
            path, _, frag = target.partition("#")
            dest = os.path.normpath(os.path.join(os.path.dirname(page),
                                                 path or os.path.basename(page)))
            if not (path == "" or os.path.exists(dest)):
                problems.append("%s -> %s (no such file)"
                                % (os.path.relpath(page, OUT), target))
            elif frag and dest.endswith((".md", ".html")) and frag not in anchors(dest):
                problems.append("%s -> %s (no such anchor)"
                                % (os.path.relpath(page, OUT), target))

    files = sum(len(n) for _b, _d, n in os.walk(OUT))
    size = sum(os.path.getsize(os.path.join(b, n))
               for b, _d, ns in os.walk(OUT) for n in ns)
    print("published tree: %d files, %.0f KB, %d md/html pages"
          % (files, size / 1024.0, len(pages)))
    for p in problems[:15]:
        print("  FAIL " + p)
    print("link check: %d problems" % len(problems))
    return 1 if problems else 0



WORKFLOW = """name: docs

on:
  push:
  pull_request:

jobs:
  links:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Every link and heading anchor resolves
        run: python3 tools/check-links.py
      - name: The rule index is valid JSON with statuses
        run: |
          python3 - <<'PY'
          import json
          d = json.load(open("docs/RULES.index", encoding="utf-8"))
          rules = d["rules"]
          allowed = {"measured", "researched only", "assumed", "contested", "procedural"}
          bad = [r["id"] for r in rules if r["status"] not in allowed]
          assert not bad, "unknown status: %s" % bad
          assert all(r["evidence"] or r["status"] != "measured" for r in rules), \
              "measured rule with no evidence"
          print("%d rules, statuses valid" % len(rules))
          PY
"""

if __name__ == "__main__":
    sys.exit(main())
