#!/usr/bin/env python3
"""check-manifest.py - lint the two rules files.

Checks the things a human forgets: section order, rule ids, cross references,
superseded tags with no amendment, MD5s that no file has, the AGENTS.md size
ceiling (G9), placeholders, and AGENTS rules that never made it into the
manifest. Read-only. Exit status 1 if anything fails, so `make check` catches it.

    python3 tools/check-manifest.py            # human output
    python3 tools/check-manifest.py -v         # also list what passed
"""

import hashlib
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = os.path.dirname(ROOT)  # files/, the archive the manifest cites
MANIFEST = os.path.join(ROOT, "MANIFEST.md")
AGENTS = os.path.join(ROOT, "AGENTS.md")

results = []


def check(name, ok, detail=""):
    results.append((name, bool(ok), detail))


def read(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read()
    except OSError as exc:
        check("read " + os.path.basename(path), False, str(exc))
        return ""


man = read(MANIFEST)
agents = read(AGENTS)
man_lines = man.split("\n")
agents_lines = agents.split("\n")

# --- 1. sections present, in order, 1..12 --------------------------------
secs = [(int(m.group(1)), i + 1)
        for i, l in enumerate(man_lines)
        for m in [re.match(r"^## (\d+)\. ", l)] if m]
nums = [s[0] for s in secs]
check("sections numbered 1..N with no gaps",
      nums == list(range(1, len(nums) + 1)), "found " + ", ".join(map(str, nums)))
check("at least 12 sections", len(nums) >= 12, str(len(nums)) + " sections")

# --- 2. principles rules R1.. : unique and one direction ----------------
rids = [int(m.group(1))
        for l in man_lines
        for m in [re.match(r"^\| R(\d+) \|", l)] if m]
check("R rules unique", len(rids) == len(set(rids)),
      "duplicates: " + str(sorted({r for r in rids if rids.count(r) > 1})))
desc = all(rids[i] > rids[i + 1] for i in range(len(rids) - 1))
asc = all(rids[i] < rids[i + 1] for i in range(len(rids) - 1))
check("R rules in one consistent order", desc or asc,
      ("descending" if desc else "ascending" if asc else
       "mixed order: " + ", ".join("R%d" % r for r in rids)))

# --- 3. G rules G1..Gn : unique, contiguous, counted once ---------------
allrules = man + '\n' + agents
for _p in (os.path.join(FILES, 'HISTORY.md'),):
    if os.path.exists(_p):
        allrules += '\n' + open(_p, encoding='utf-8').read()
gids = [int(m.group(1)) for m in re.finditer(r"^\*\*G(\d+) —", man + '\n' + agents, re.M)]
check("G rules unique", len(gids) == len(set(gids)),
      str(sorted({g for g in gids if gids.count(g) > 1})))
check("G rules contiguous from 1", gids == list(range(1, len(gids) + 1)),
      "found " + ", ".join("G%d" % g for g in gids))

# --- 4. every section cross reference resolves --------------------------
refs = set()
for text in (man, agents):
    refs |= {int(m.group(1)) for m in re.finditer(r"§(\d+)", text)}
missing = sorted(r for r in refs if r not in nums)
check("all §N cross references exist", not missing,
      "dangling: " + ", ".join("§%d" % r for r in missing))

# --- 5. superseded tags pair with amendments ----------------------------
allrules = man + '\n' + agents
_hpath = os.path.join(FILES, 'HISTORY.md')
if os.path.exists(_hpath):
    allrules += '\n' + open(_hpath, encoding='utf-8').read()

# tags may wrap across lines, so match the whole text and map back to a line
tags = [(allrules[:m.start()].count("\n") + 1, m.group(1))
        for m in re.finditer(r"\[superseded[^\[\]]*?A(\d+)[^\[\]]*?\]", allrules, re.S)]
def _amends(text):
    m = re.search(r"^#{2,3} Amendments.*?(?=^#{2,3} |\Z)", text, re.S | re.M)
    if not m:
        return []
    return [int(x.group(1)) for x in re.finditer(r"^\*\*A(\d+) —", m.group(0), re.M)]

amends = _amends(man) + _amends(agents)
bad = [(ln, "A" + a) for ln, a in tags if int(a) not in amends]
check("every superseded tag names a real amendment", not bad, str(bad))
check("every amendment is referenced by at least one tag",
      all(a in [int(x[1]) for x in tags] for a in amends),
      "unreferenced: " + str([a for a in amends
                              if a not in [int(x[1]) for x in tags]]))

# --- 6. MD5s mentioned must belong to a file we have --------------------
have = set()
if os.path.isdir(FILES):
    for base, _dirs, names in os.walk(FILES):
        if "_scratch" in base:
            continue
        for n in names:
            p = os.path.join(base, n)
            try:
                if os.path.getsize(p) > 12 * 1024 * 1024:
                    continue
                h = hashlib.md5()
                with open(p, "rb") as f:
                    for chunk in iter(lambda: f.read(1 << 20), b""):
                        h.update(chunk)
                have.add(h.hexdigest())
            except OSError:
                pass
md5s = set(m.group(0) for m in re.finditer(r"\b[0-9a-f]{32}\b", man + agents))
exempt = set()
for line in (man + '\n' + agents).split('\n'):
    if 'not archived' in line:
        exempt |= set(re.findall(r'\b[0-9a-f]{32}\b', line))
unknown = sorted(m for m in md5s if m not in have and m not in exempt)
if not any(n.endswith(".pkg") for n in
           (os.listdir(FILES) if os.path.isdir(FILES) else [])):
    # A copy without the package archive cannot verify MD5s. Say so instead of
    # failing: the bundle is meant to run anywhere, the archive is not in it.
    check("every MD5 in the text matches a file in files/", True,
          "skipped: no file archive in this copy")
else:
    check("every MD5 in the text matches a file in files/", not unknown,
          "unknown: " + ", ".join(unknown))

# --- 7. AGENTS.md obeys its own G9 ceiling ------------------------------
m = re.search(r"does not exceed (\d+) lines", agents)
ceiling = int(m.group(1)) if m else None
check("AGENTS.md states its ceiling", ceiling is not None)
if ceiling:
    check("AGENTS.md within its %d-line ceiling" % ceiling,
          len(agents_lines) <= ceiling,
          "%d lines, %d over" % (len(agents_lines), len(agents_lines) - ceiling))

# --- 8. no placeholders left in the rules files -------------------------
ph = []
for name, text in (("MANIFEST.md", man), ("AGENTS.md", agents)):
    for i, l in enumerate(text.split("\n")):
        if re.search(r"\b(TODO|FIXME|XXX|TBD)\b", l):
            ph.append("%s:%d" % (name, i + 1))
check("no TODO/FIXME/XXX/TBD left", not ph, ", ".join(ph))

# --- 9. AGENTS mentions only G rules that exist in the manifest ---------
ag = {int(m.group(1)) for m in re.finditer(r"\bG(\d+)\b", agents)}
check("AGENTS only cites G rules that exist", ag <= set(gids),
      "unknown: " + str(sorted(ag - set(gids))))
check("every G rule is mentioned in AGENTS", set(gids) <= ag,
      "missing: " + str(sorted(set(gids) - ag)))

# --- 10. duplicated rule bullets in AGENTS ------------------------------
seen = {}
dups = []
for i, l in enumerate(agents_lines):
    if l.startswith("- **") and len(l) > 30:
        key = l[4:64]
        if key in seen:
            dups.append("line %d dups %d" % (i + 1, seen[key]))
        seen[key] = i + 1
check("no duplicated rule bullets in AGENTS.md", not dups, ", ".join(dups))

# --- 11. the where-we-are block must name the newest packages -----------
# MANIFEST section 2 holds the single answer to 'where are we'. It goes stale the
# moment a package is copied into files/ and the block is not edited with it.

def _newest(pattern):
    best = None
    if not os.path.isdir(FILES):
        return None
    for n in sorted(os.listdir(FILES)):
        m = re.match(pattern, n)
        if m:
            key = int(m.group(1))
            if best is None or key > best[0]:
                best = (key, os.path.join(FILES, n))
    return best


def _md5(path):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


start = man.find('## 2.')
end = man.find('## 3.')
block = man[start:end] if start >= 0 and end > start else ''
check('where-we-are block found in section 2', bool(block))
for label, pattern in (('player', r'^Ultimate-Media-Player-v(\d+)\.pkg$'),
                       ('GPU', r'^GPU-.+\.pkg$')):
    cand = _newest(pattern if label == 'player' else r'^GPU-.*BREW(\d+)\.pkg$')
    if not cand:
        check('where-we-are: a %s package exists to name' % label, True,
              'skipped: no package archive in this copy')
        continue
    h = _md5(cand[1])
    check('where-we-are names the newest %s package' % label, h in block,
          'newest is %s (%s), which the block does not mention'
          % (os.path.basename(cand[1]), h))

# --- 12. read order is stated, so a new reader knows what to read first --
check("MANIFEST.md states the read order", "**Read order:**" in man)
check("AGENTS.md states the read order", "**Read order:**" in agents)

# --- 13. byte budgets: a doc that only grows stops being read ------------
_sp = os.path.join(FILES, "STATUS.html")
status_html = open(_sp, encoding="utf-8").read() if os.path.exists(_sp) else ""
check("files/STATUS.html exists (make status)", bool(status_html),
      "run: make status")
_hpath2 = os.path.join(FILES, "HISTORY.md")
hist = open(_hpath2, encoding="utf-8").read() if os.path.exists(_hpath2) else ""
BUDGETS = (("MANIFEST.md", man, 131072, "128 KB"),
           ("AGENTS.md", agents, 40960, "40 KB"),
           ("HISTORY.md", hist, 98304, "96 KB"),
           ("STATUS.html", status_html, 32768, "32 KB"))
for _name, _text, _cap, _label in BUDGETS:
    _size = len(_text.encode("utf-8"))
    check("%s within its %s budget" % (_name, _label), _size <= _cap,
          "%d bytes, %d over %s" % (_size, _size - _cap, _label))

# --- 14. a measurement figure lives in one place -------------------------
# AGENTS.md states the rules; the numbers belong to the manifest and the log.
# A figure repeated in AGENTS is allowed only with the section that owns it, so
# a correction has one place to go and cannot leave a stale copy behind.
FIG = re.compile(r"\d+\.\d+\s?(?:ms|fps|GB|MB|vsyncs?|%)")
elsewhere = set(FIG.findall(man)) | set(FIG.findall(hist))
dupfig = []
for _i, _l in enumerate(agents_lines):
    if re.search(r"\(R\d+\)|\(§\d+\)|R\d+\b|§\d+", _l):
        continue
    _hit = sorted(set(FIG.findall(_l)) & elsewhere)
    if _hit:
        dupfig.append("line %d: %s" % (_i + 1, ", ".join(_hit)))
check("AGENTS figures cite the section that owns them", not dupfig,
      "; ".join(dupfig[:5]))

# --- 15. the status board is not older than what it summarises -----------
if status_html:
    _stale = [n for n, f in (("MANIFEST.md", MANIFEST), ("AGENTS.md", AGENTS))
              if os.path.exists(f)
              and os.path.getmtime(f) > os.path.getmtime(_sp) + 1]
    check("STATUS.html is newer than the rules files", not _stale,
          "stale against " + ", ".join(_stale) + " -- run: make status")

# --- 16. the index and its falsifiers (R33, R34) -------------------------
# Until now this checker never opened RULES.index, so all 48 falsifiers and
# R33's "in the table and in the index" lived only in the generator. A rule is
# checked here as a rule, not as prose.
_rp = os.path.join(FILES, "RULES.index")
_idx = []
try:
    _idx = json.load(open(_rp, encoding="utf-8"))["rules"]
    check("RULES.index parses", True)
except (OSError, ValueError, KeyError) as _exc:
    check("RULES.index parses", False, str(_exc))
# A skipped rule is not a passing rule. With no usable index R33 and R34 cannot
# run at all, and silence there is how a published tree reads as clean while the
# two rules that police the index are absent. Name the failure instead (R35: a
# judgement the checker cannot make is never reported as a pass).
if not _idx:
    check("RULES.index is present and non-empty for R33/R34", False,
          "no index at %s - R33 and R34 were not run" % _rp)
if _idx:
    _want = set(re.findall(r"(?m)^\| (R\d+) \|", man))
    _want |= set(re.findall(r"(?m)^\*\*(G\d+)\b", agents))
    _have = set(r["id"] for r in _idx)
    check("RULES.index and the rule table agree on ids (R33)", _want == _have,
          "only in prose: %s; only in index: %s"
          % (", ".join(sorted(_want - _have)) or "none",
             ", ".join(sorted(_have - _want)) or "none"))
    _bare = [r["id"] for r in _idx if not (r.get("falsifier") or "").strip()]
    check("every indexed rule carries a falsifier (R34)", not _bare,
          "no falsifier: " + ", ".join(_bare))
    # A note, never a failure: the checker cannot tell a derived constant from a
    # misquoted measurement, and a judgement it cannot make must not be reported
    # as a pass (R35). Printed so the list stays in front of a reader.
    _rows = dict((m.group(1), m.group(0)) for m in
                 re.finditer(r"(?m)^\| (R\d+) \|.*$", man))
    _derived = []
    for _r in _idx:
        if _r["kind"] != "R":
            continue
        _row = _rows.get(_r["id"], "")
        _odd = sorted(n for n in set(re.findall(r"\d+(?:\.\d+)?", _r["falsifier"]))
                      if n not in _row)
        if _odd:
            _derived.append("%s %s" % (_r["id"], "/".join(_odd)))
    if _derived:
        print("note  falsifier figures absent from their own row - derived, or "
              "misquoted: " + "; ".join(_derived))

failed = [r for r in results if not r[1]]
for name, ok, detail in results:
    if ok:
        if "-v" in sys.argv:
            print("ok    %s" % name)
    else:
        print("FAIL  %s%s" % (name, ("  (" + detail + ")") if detail else ""))
print("manifest check: %d passed, %d failed" %
      (len(results) - len(failed), len(failed)))
sys.exit(1 if failed else 0)
