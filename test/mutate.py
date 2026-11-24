#!/usr/bin/env python3
"""mutate.py - inject one fault at a time and prove a checker notices it.

Every case runs against a throwaway copy of the document tree under
tempfile.mkdtemp(), never the working tree. The old harness lived in
_scratch/linttest/, and a scratch clean deleted it; a harness that can vanish
silently is worse than none, so this one lives in test/ beside the player tests.

A case counts as caught only when the real checker exits non-zero *and* prints
the failure it is meant to print. A crash for an unrelated reason is not a catch.

    python3 test/mutate.py          # 12 cases
    python3 test/mutate.py -v       # also show the line that matched
"""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                  # ps4-media-player/
FILES = os.path.dirname(ROOT)                 # kit root: HISTORY.md, RULES.index
DOCS = ("MANIFEST.md", "AGENTS.md")
ARCHIVE = ("HISTORY.md", "STATUS.html", "RULES.index")
VERBOSE = "-v" in sys.argv

# The harness copies the kit's documents into a throwaway tree and injects faults
# into them. In the published tree those documents live under docs/, so there is
# nothing to copy: report that and exit without touching the disk.
_missing = [n for n in DOCS if not os.path.exists(os.path.join(ROOT, n))]
if _missing:
    print("not runnable here: %s not found in %s" % (", ".join(_missing), ROOT))
    print("the harness copies the kit's documents; the published tree keeps them under docs/.")
    print("nothing was copied and no case was run.")
    sys.exit(2)


# --- the copy -------------------------------------------------------------

def make_tree():
    tmp = tempfile.mkdtemp(prefix="mutate-")
    repo = os.path.join(tmp, "repo")
    os.makedirs(repo)
    for n in DOCS:
        shutil.copy2(os.path.join(ROOT, n), os.path.join(repo, n))
    shutil.copytree(os.path.join(ROOT, "tools"), os.path.join(repo, "tools"),
                    ignore=shutil.ignore_patterns("__pycache__"))
    for n in ARCHIVE:
        src = os.path.join(FILES, n)
        if os.path.exists(src):
            shutil.copy2(src, os.path.join(tmp, n))
    return tmp, repo


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def write_keep_mtime(path, text):
    """Write a fault but keep the file's timestamp.

    The board's staleness check compares mtimes, so a plain write would trip a
    second, unrelated check. The fault must be noticed on its own merits.
    """
    st = os.stat(path)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    os.utime(path, (st.st_atime, st.st_mtime))


def repo_file(repo, name):
    return os.path.join(repo, name)


def archive_file(repo, name):
    return os.path.join(os.path.dirname(repo), name)


def run_checker(repo, script):
    p = subprocess.run([sys.executable, os.path.join("tools", script)],
                       cwd=repo, capture_output=True, text=True, timeout=120)
    return p.returncode, (p.stdout + p.stderr).strip()


# --- the faults -----------------------------------------------------------

def f_section_gap(repo):
    p = repo_file(repo, "MANIFEST.md")
    s = read(p)
    new = re.sub(r"(?m)^## 2\. ", "## 99. ", s, count=1)
    assert new != s, "no '## 2.' heading to break"
    write_keep_mtime(p, new)


def f_duplicate_rule_id(repo):
    p = repo_file(repo, "MANIFEST.md")
    lines = read(p).split("\n")
    for i, l in enumerate(lines):
        if re.match(r"^\| R\d+ \|", l):
            lines.insert(i + 1, l)
            break
    else:
        raise AssertionError("no R row to duplicate")
    write_keep_mtime(p, "\n".join(lines))


def f_dangling_reference(repo):
    p = repo_file(repo, "MANIFEST.md")
    write_keep_mtime(p, read(p) + "\nA dangling \u00a797 reference.\n")


def f_placeholder(repo):
    p = repo_file(repo, "AGENTS.md")
    write_keep_mtime(p, read(p) + "\nTODO: replace this with something real.\n")


def f_unpaired_superseded(repo):
    p = repo_file(repo, "MANIFEST.md")
    write_keep_mtime(p, read(p) + "\n[superseded by A97]\n")


def f_agents_over_budget(repo):
    p = repo_file(repo, "AGENTS.md")
    # One long line, so the line ceiling is not what trips - only the byte budget.
    write_keep_mtime(p, read(p) + "z" * 900 + "\n")


def f_duplicate_bullet(repo):
    p = repo_file(repo, "AGENTS.md")
    lines = read(p).split("\n")
    for l in lines:
        if l.startswith("- **") and len(l) > 30:
            lines.append(l)
            break
    else:
        raise AssertionError("no rule bullet to duplicate")
    write_keep_mtime(p, "\n".join(lines))


def f_index_unparsable(repo):
    p = archive_file(repo, "RULES.index")
    with open(p, "w", encoding="utf-8") as f:
        f.write("{")


def _load_index(repo):
    p = archive_file(repo, "RULES.index")
    data = json.loads(read(p))
    return p, data


def f_index_missing_id(repo):
    p, data = _load_index(repo)
    first_r = next(r for r in data["rules"] if r["kind"] == "R")
    data["rules"] = [r for r in data["rules"] if r["id"] != first_r["id"]]
    data["count"] = len(data["rules"])
    with open(p, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1, ensure_ascii=False)


def f_empty_falsifier(repo):
    p, data = _load_index(repo)
    first_r = next(r for r in data["rules"] if r["kind"] == "R")
    first_r["falsifier"] = ""
    with open(p, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1, ensure_ascii=False)


def f_number_without_status(repo):
    p = repo_file(repo, "MANIFEST.md")
    s = read(p)
    ids = [int(x) for x in re.findall(r"(?m)^\| R(\d+) \|", s)]
    row = "\n| R%d | the render path costs 7.5 ms per frame | provisional | none yet |\n" % (max(ids) + 1)
    write_keep_mtime(p, s + row)


CASES = (
    ("section numbering has a gap", "MANIFEST.md", "check-manifest.py",
     "sections numbered 1..N with no gaps", f_section_gap),
    ("the same R id appears twice", "MANIFEST.md", "check-manifest.py",
     "R rules unique", f_duplicate_rule_id),
    ("a cross reference points nowhere", "MANIFEST.md", "check-manifest.py",
     "all \u00a7N cross references exist", f_dangling_reference),
    ("a placeholder is left behind", "AGENTS.md", "check-manifest.py",
     "no TODO/FIXME/XXX/TBD left", f_placeholder),
    ("superseded tag names no amendment", "MANIFEST.md", "check-manifest.py",
     "every superseded tag names a real amendment", f_unpaired_superseded),
    ("AGENTS grows past its 40 KB budget", "AGENTS.md", "check-manifest.py",
     "AGENTS.md within its 40 KB budget", f_agents_over_budget),
    ("a rule bullet is duplicated", "AGENTS.md", "check-manifest.py",
     "no duplicated rule bullets in AGENTS.md", f_duplicate_bullet),
    ("RULES.index is not valid JSON", "RULES.index", "check-manifest.py",
     "RULES.index parses", f_index_unparsable),
    ("index drops a rule the prose keeps", "RULES.index", "check-manifest.py",
     "RULES.index and the rule table agree on ids (R33)", f_index_missing_id),
    ("an indexed rule has no falsifier", "RULES.index", "check-manifest.py",
     "every indexed rule carries a falsifier (R34)", f_empty_falsifier),
    ("a number is quoted with no status word", "MANIFEST.md", "rule-index.py",
     "quotes a number with no status word", f_number_without_status),
)


# --- case 12: the build stamp must not read the clock ---------------------

def buildstamp_is_clockproof(script):
    """True when two runs a second apart agree and epoch 0 is honoured."""
    env = dict(os.environ)
    env.pop("SOURCE_DATE_EPOCH", None)

    def run(extra=None):
        e = dict(env)
        if extra:
            e.update(extra)
        p = subprocess.run([sys.executable, script], capture_output=True,
                           text=True, env=e, timeout=60)
        return p.returncode, p.stdout.strip()

    first = run()
    time.sleep(1.1)
    second = run()
    epoch = run({"SOURCE_DATE_EPOCH": "0"})
    return (first[0] == 0 and first[1] == second[1]
            and "1970-01-01" in epoch[1] and "00:00:00" in epoch[1])


def clockproof_case(tmp):
    """The property has teeth only if a clock-reading version fails it."""
    real = os.path.join(ROOT, "tools", "buildstamp.py")
    if not buildstamp_is_clockproof(real):
        return False, "the real tool already fails its own property"
    mutant = os.path.join(tmp, "buildstamp-clock.py")
    src = read(real)
    broken = src.replace(
        'DEFAULT = ("2026-11-24", "00:00:00")',
        'DEFAULT = (time.strftime("%Y-%m-%d"), time.strftime("%H:%M:%S"))')
    if broken == src:
        return False, "could not rewrite DEFAULT, so the mutant was never built"
    with open(mutant, "w", encoding="utf-8") as f:
        f.write(broken)
    if buildstamp_is_clockproof(mutant):
        return False, "a clock-reading buildstamp passed the property"
    return True, "a clock-reading buildstamp fails the property"


# --- driver ---------------------------------------------------------------

def main():
    print("Mutation check: each fault must be caught by the real checker")
    print("")

    tmp, repo = make_tree()
    baseline = []
    try:
        for script in ("check-manifest.py", "rule-index.py"):
            code, out = run_checker(repo, script)
            baseline.append((script, code, out))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    for script, code, out in baseline:
        if code != 0:
            print("baseline is red: %s fails on an unmutated copy" % script)
            for line in out.splitlines():
                if line.startswith("FAIL") or "problems" in line or "failed" in line:
                    print("  " + line)
            print("mutate: 0 of %d caught - fix the baseline first" % (len(CASES) + 1))
            return 1

    caught = 0
    total = len(CASES) + 1
    for i, (name, where, script, expect, apply_fault) in enumerate(CASES, 1):
        tmp, repo = make_tree()
        try:
            try:
                apply_fault(repo)
            except AssertionError as exc:
                print("MISS  %2d/%d  %-42s %s" % (i, total, name, exc))
                continue
            code, out = run_checker(repo, script)
            hit = [l for l in out.splitlines() if expect in l]
            if code != 0 and hit:
                caught += 1
                print("caught %2d/%d  %-42s %s" % (i, total, name, where))
                if VERBOSE:
                    print("        " + hit[0])
            else:
                print("MISS  %2d/%d  %-42s exit %d, expected %r"
                      % (i, total, name, code, expect))
                for line in out.splitlines()[-3:]:
                    print("        " + line)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    tmp = tempfile.mkdtemp(prefix="mutate-")
    try:
        ok, why = clockproof_case(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if ok:
        caught += 1
        print("caught %2d/%d  %-42s %s" % (total, total,
              "build stamp does not read the clock", why))
    else:
        print("MISS  %2d/%d  %-42s %s" % (total, total,
              "build stamp does not read the clock", why))

    print("")
    print("mutate: %d of %d caught" % (caught, total))
    return 0 if caught == total else 1


if __name__ == "__main__":
    sys.exit(main())
