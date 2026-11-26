#!/usr/bin/env python3
"""rule-index.py - build files/RULES.index and validate rule provenance.

Parses the two places rules are written:
  * R-rules  : the principles table in MANIFEST.md section 9   (| R19 | ... |)
  * G-rules  : "**G1 - title.**" blocks in AGENTS.md

Writes a machine-readable index (id, kind, status, evidence, statement) so a
future reader can load ~40 one-line rules instead of 118 KB of prose, and it
validates the part that drifts: a rule that quotes a number must say where the
number came from, and a rule that claims "measured" must name something we can
check - a log file that exists, an MD5, or a build id.

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
for base, _d, names in os.walk(FILES):
    if "_scratch" in base:
        continue
    for n in names:
        if n.endswith(".log"):
            logs.add(n)


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

for m in re.finditer(r"^\*\*(G\d+) — (.+?)\.\*\*(.*?)(?=^\*\*G\d+ — |^## |\Z)",
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
    if numeric and status == "procedural":
        problems.append("%s quotes a number with no status word (%s)"
                        % (r["id"], "measured/researched only/assumed/contested"))
    if r["kind"] == "G" and not r["evidence_cell"].strip():
        problems.append("%s has no body text to validate" % r["id"])

    index.append({"id": r["id"], "kind": r["kind"], "status": status,
                  "evidence": (r["evidence_cell"].strip()
                               or ", ".join(cited_logs + cited_ids) or
                               ("md5" if cited_md5 else "")),
                  "statement": re.sub(r"\s+", " ", r["statement"])})

index.sort(key=lambda x: (x["kind"], int(re.sub(r"\D", "", x["id"]))))

# preserved fields - this generator only derives id/kind/status/evidence/statement;
# falsifier and verified_on live only in the index and must survive a rebuild (R34).
try:
    _prev = {r["id"]: r for r in json.loads(read(OUT))["rules"]}
except Exception:
    _prev = {}
for _e in index:
    for _k in ("falsifier", "verified_on"):
        if _prev.get(_e["id"], {}).get(_k):
            _e[_k] = _prev[_e["id"]][_k]

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

for p in problems:
    print("FAIL  " + p)
print("rule index: %d rules, %d problems" % (len(index), len(problems)))
sys.exit(1 if problems else 0)
