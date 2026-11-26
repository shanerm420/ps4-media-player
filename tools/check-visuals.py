#!/usr/bin/env python3
"""check-visuals.py - the visualizer gate, run by `make check`.

The register (docs/visualizers.csv) says which looks exist, whether each one is
still the legacy scene, a rebuilt scene or one of the owner's approved
references, its budget, and the render that proves it was looked at.

This checks what can be checked without a console:

  1. the register covers styles 0-17, once each;
  2. every status is one of legacy / rebuilt / reference;
  3. every row names a render, and a rebuilt or reference row's render exists;
  4. no look claims a budget above the visualizer limit in AGENTS.md (50 ms);
  5. if files/bench/visualizers.txt exists, every rebuilt look has a line in it
     and is inside its budget. A missing bench line is a **failure**, not a
     note: until 2026-11-25 this file promised "unmeasured, never as passing"
     and then exited 0 on an unmeasured look, which is the class of claim this
     whole audit is about (G3).
  6. the bench is not older than the renderer it claims to measure. `files/bench`
     said 11 looks were inside budget while `src/vis_art.c` had been edited
     after the numbers were taken; nothing compared the two dates.

Exit 0 with "0 problems", or 1 with one FAIL line per problem.
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REGISTER = os.path.join(ROOT, "docs", "visualizers.csv")
BENCH = os.path.join(ROOT, "files", "bench", "visualizers.txt")
LIMIT_MS = 50.0
STATUSES = {"legacy", "rebuilt", "reference"}
STYLES = 18


def fail(problems, msg):
    problems.append("FAIL  " + msg)


def main():
    problems = []
    notes = []

    if not os.path.exists(REGISTER):
        print("FAIL  docs/visualizers.csv is missing")
        return 1

    with open(REGISTER, newline="") as f:
        rows = list(csv.DictReader(f))

    seen = {}
    for row in rows:
        name = (row.get("name") or "?").strip()
        try:
            style = int(row["style"])
        except (KeyError, ValueError):
            fail(problems, "%s has no numeric style" % name)
            continue
        if style in seen:
            fail(problems, "style %d registered twice (%s and %s)"
                 % (style, seen[style], name))
        seen[style] = name

        status = (row.get("status") or "").strip()
        if status not in STATUSES:
            fail(problems, "%s has status %r, not one of %s"
                 % (name, status, sorted(STATUSES)))

        try:
            budget = float(row.get("budget_ms") or "")
        except ValueError:
            fail(problems, "%s has no numeric budget_ms" % name)
            budget = LIMIT_MS
        if budget > LIMIT_MS:
            fail(problems, "%s claims %.1f ms, above the %.0f ms limit"
                 % (name, budget, LIMIT_MS))

        render = (row.get("render") or "").strip()
        if not render:
            fail(problems, "%s names no render" % name)
        elif status in ("rebuilt", "reference"):
            path = os.path.normpath(os.path.join(ROOT, render))
            if not os.path.exists(path):
                fail(problems, "%s render is missing: %s" % (name, render))

    for style in range(STYLES):
        if style not in seen:
            fail(problems, "style %d is not registered" % style)

    # the bench lines, when a round has produced them
    if os.path.exists(BENCH):
        bench = {}
        with open(BENCH) as f:
            for line in f:
                line = line.strip()
                if not line.startswith("style="):
                    continue
                parts = dict(p.split("=", 1) for p in line.split() if "=" in p)
                try:
                    bench[int(parts["style"])] = float(parts["ms"])
                except (KeyError, ValueError):
                    continue
        for row in rows:
            if (row.get("status") or "").strip() != "rebuilt":
                continue
            name = row.get("name")
            style = int(row["style"])
            budget = float(row.get("budget_ms") or LIMIT_MS)
            if style not in bench:
                fail(problems, "%s is unmeasured (no bench line) - run make bench-vis"
                     % name)
            elif bench[style] > budget:
                fail(problems, "%s is %.1f ms, over its %.0f ms budget"
                     % (name, bench[style], budget))
    else:
        fail(problems, "no bench file yet (run: make bench-vis)")

    # The numbers only describe the renderer that produced them. A benchmark
    # older than the drawing code is a claim about code that is no longer here.
    if os.path.exists(BENCH):
        bench_t = os.path.getmtime(BENCH)
        for src in ("src/vis_art.c", "src/spectrum.c"):
            p = os.path.join(ROOT, src)
            if os.path.exists(p) and os.path.getmtime(p) > bench_t + 1:
                fail(problems, "files/bench is older than %s - run make bench-vis" % src)

    for n in notes:
        print(n)
    for p in problems:
        print(p)
    print("visuals check: %d registered, %d rebuilt, %d problems"
          % (len(rows),
             sum(1 for r in rows if (r.get("status") or "").strip() == "rebuilt"),
             len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
