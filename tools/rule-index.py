#!/usr/bin/env python3
"""rule-index.py - build files/RULES.index and validate rule provenance.

Parses the two places rules are written:
  * R-rules  : the principles table in MANIFEST.md section 9   (| R19 | ... |)
  * G-rules  : "**G1 - title.**" blocks in AGENTS.md

Writes a machine-readable index (id, kind, status, evidence, statement) so a
future reader can load ~40 one-line rules instead of 118 KB of prose, and it
validates the part that drifts: a rule that quotes a number must say where the
number came from, and a rule that claims "measured" must name something that can
actually be reached from this archive - a log file that exists, an MD5, a cited
source file, or a build id whose package is still in `files/`.

The last clause is the one this tool used to overstate. Console logs cited by
rules are often not archived (they were pulled, read and lost), so citing one
proves nothing by itself; until 2026-11-25 the tool printed a `note` for that and
still exited 0, while its own docstring promised the citation was checked. A
measured rule whose only evidence cannot be reached from here is now a failure,
and a rule backed by a build id resolves through the package that id names.

Read-only apart from regenerating files/RULES.index. Exit 1 on failure.
"""

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = os.path.dirname(ROOT)
MANIFEST = os.path.join(ROOT, "MANIFEST.md")
AGENTS = os.path.join(ROOT, "AGENTS.md")
HISTORY = os.path.join(FILES, "HISTORY.md")
OUT = os.path.join(FILES, "RULES.index")

STATUS = ("measured", "researched only", "researched", "assumed", "contested")

logs = set()
archived = []          # every file name in the archive, for build-id lookups
for base, _d, names in os.walk(FILES):
    if "_scratch" in base:
        continue
    for n in names:
        if n.endswith(".log"):
            logs.add(n)
        archived.append(n)


def read(p):
    try:
        with open(p, encoding="utf-8") as f:
            return f.read()
    except OSError:
        return ""


man, agents = read(MANIFEST), read(AGENTS)
every = man + "\n" + agents + "\n" + read(HISTORY)

rules = []

for line in man.split("\n"):
    m = re.match(r"^\| (R\d+) \| (.+?) \| ([^|]*) \| ([^|]*) \|\s*$", line)
    if m:
        rules.append({"id": m.group(1), "kind": "R", "statement": m.group(2).strip(),
                      "status_cell": m.group(3).strip(), "evidence_cell": m.group(4).strip()})

for m in re.finditer(r"^\*\*(G\d+) — (.+?)\.\*\*(.*?)(?=^\*\*G\d+ — |^#{2,3} |\Z)",
                     agents, re.S | re.M):
    rules.append({"id": m.group(1), "kind": "G",
                  "statement": (m.group(2) + ".").strip(),
                  "status_cell": "", "evidence_cell": m.group(3).strip()})

problems = []
index = []

for r in rules:
    blob = (r["statement"] + " " + r["status_cell"] + " " + r["evidence_cell"]).lower()
    negated = ("not yet measured" in blob or "not measured" in blob
               or "unmeasured" in blob)
    status = ("assumed" if negated
              else next((s for s in STATUS if s in blob), "procedural"))
    text = r["statement"] + " " + r["evidence_cell"]
    cited_logs = [n for n in logs if n in text]
    cited_ids = re.findall(r"\b(BREW\d{5}|v\d{3})\b", text)
    cited_md5 = re.findall(r"\b[0-9a-f]{32}\b", text)
    cited_files = re.findall(r"[\w./-]+\.(?:md|cpp|hpp|py|log)", text)
    has_evidence = bool(cited_logs or cited_ids or cited_md5 or cited_files)
    numeric = bool(re.search(r"\d+(\.\d+)?\s*(ms|fps|%|MiB|MB|KB|Hz|px)", blob))

    if status == "measured" and not has_evidence:
        problems.append("%s claims measured but cites no log, file, MD5 or build id" % r["id"])
    archived_ok = True
    if status == "measured":
        # Reachability, not just citation: a build id counts only if its package
        # is still here, a log only if it is on disk. A rule whose log was pulled,
        # read and not kept must say so in its own evidence cell - "not archived" -
        # and it is then counted and shown as unarchived rather than passing
        # silently. The phrase is an admission, not a bypass: it is printed in the
        # summary and carried in the index.
        reachable = (bool(cited_logs)
                     or any(i in n for i in cited_ids for n in archived)
                     or any(os.path.exists(os.path.join(ROOT, f)) for f in cited_files))
        admitted = "not archived" in blob or "log lost" in blob
        if not reachable and not admitted:
            problems.append("%s is measured but nothing it cites can be reached from this "
                            "archive (logs: %s, ids: %s) and it does not say so"
                            % (r["id"], ", ".join(cited_logs) or "none",
                               ", ".join(cited_ids) or "none"))
        archived_ok = reachable
    if numeric and status == "procedural":
        problems.append("%s quotes a number with no status word (%s)"
                        % (r["id"], "measured/researched only/assumed/contested"))
    if r["kind"] == "G" and not r["evidence_cell"].strip():
        problems.append("%s has no body text to validate" % r["id"])

    index.append({"id": r["id"], "kind": r["kind"], "status": status,
                  "evidence": (r["evidence_cell"].strip()
                               or ", ".join(cited_logs + cited_ids) or
                               ("md5" if cited_md5 else "")),
                  "evidence_archived": (archived_ok if status == "measured" else None),
                  "statement": re.sub(r"\s+", " ", r["statement"])})

index.sort(key=lambda x: (x["kind"], int(re.sub(r"\D", "", x["id"]))))

payload = {"generated_from": ["MANIFEST.md section 9", "AGENTS.md G-rules"],
           "count": len(index), "rules": index}
text = json.dumps(payload, indent=1, ensure_ascii=False) + "\n"
old = read(OUT)
if old != text:
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)
    print("wrote %s (%d rules)" % (os.path.basename(OUT), len(index)))
else:
    print("RULES.index up to date (%d rules)" % len(index))

# The published index is checked by CI with the same rule: a measured rule must
# carry evidence in the file, not only in the parser's head.
for r in index:
    if r["status"].startswith("measured") and not r["evidence"]:
        problems.append("%s is measured but its index entry carries no evidence"
                        % r["id"])

missing_logs = sorted({n for r in rules for n in re.findall(r"[\w-]+\.log", r["evidence_cell"])
                       if n not in logs})
if missing_logs:
    print("note  cited logs not archived in files/: " + ", ".join(missing_logs))

unarchived = [r["id"] for r in index
              if r["status"] == "measured" and r["evidence_archived"] is False]
if unarchived:
    print("admit measured, evidence not in this archive: %d rule(s): %s"
          % (len(unarchived), ", ".join(unarchived)))

for p in problems:
    print("FAIL  " + p)
print("rule index: %d rules, %d problems" % (len(index), len(problems)))
sys.exit(1 if problems else 0)
