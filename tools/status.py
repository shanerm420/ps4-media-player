#!/usr/bin/env python3
"""status.py - run every host-side check and write files/STATUS.html.

One page that answers: is the system healthy, where is the project, what is still
waiting on the console. Runs the two checkers, the mutation harness and the audio
tests, pulls the current state out of the documents, and renders it all as a
single self-contained HTML file with no external dependencies.

    python3 tools/status.py            # write files/STATUS.html
    python3 tools/status.py --serve    # serve it on 0.0.0.0:3000
"""

import hashlib
import html
import json
import os
import re
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = os.path.dirname(ROOT)
OUT = os.path.join(FILES, "STATUS.html")


def run(cmd, cwd=ROOT, timeout=300):
    t0 = time.time()
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                           timeout=timeout)
        return p.returncode, (p.stdout + p.stderr).strip(), time.time() - t0
    except subprocess.TimeoutExpired:
        return 124, "timed out after %ds" % timeout, time.time() - t0


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def newest(pattern):
    best = None
    for n in os.listdir(FILES):
        m = re.match(pattern, n)
        if m and n.endswith(".pkg"):
            key = int(m.group(1))
            p = os.path.join(FILES, n)
            if best is None or key > best[0]:
                best = (key, n, p)
    return best


# Touch the board before running the checks: the linter fails a board older than
# the files it summarises, and this run is about to rewrite it anyway. Without
# this the board's own lint row would be red on the first run after an edit.
try:
    open(OUT, "w", encoding="utf-8").write("<!doctype html><title>generating</title>")
except OSError:
    pass

steps = []


def step(name, what, cmd):
    code, out, secs = run(cmd)
    steps.append({"name": name, "what": what, "ok": code == 0,
                  "out": out, "secs": secs, "cmd": " ".join(cmd)})
    return code == 0


step("Rules lint", "sections, rule ids, cross references, MD5s, ceilings",
     [sys.executable, "tools/check-manifest.py"])
step("Rule index", "every rule parsed, each status checked against its evidence",
     [sys.executable, "tools/rule-index.py"])
step("Checker self-test", "12 injected faults, each one must be caught",
     [sys.executable, "test/mutate.py"])
step("Player tests", "host-side audio path, no console needed",
     ["make", "-C", "test", "run"])

man = open(os.path.join(ROOT, "MANIFEST.md"), encoding="utf-8").read()
agents = open(os.path.join(ROOT, "AGENTS.md"), encoding="utf-8").read()

block = ""
if "## 2." in man and "## 3." in man:
    block = man[man.index("## 2."):man.index("## 3.")]


def field(name):
    m = re.search(r"^\| %s \| (.+?) \|\s*$" % re.escape(name), block, re.M)
    return m.group(1).strip() if m else ""


def plain(text):
    """The cards are plain HTML: markdown emphasis and code ticks would show as
    literal asterisks and backticks, which reads as rubble rather than emphasis."""
    return text.replace("**", "").replace("`", "")


pl = newest(r"Ultimate-Media-Player-v(\d+)\.pkg$")
gp = newest(r"GPU-.*BREW(\d+)\.pkg$")
player = ("v%s — %s" % (pl[0], "`" + md5(pl[2]) + "`")) if pl else "none found"
gpu = ("BREW%05d — %s" % (gp[0], "`" + md5(gp[2]) + "`")) if gp else "none found"

# The register lives in APPENDIX-LOGS.md since 2026-11-24 (G9); reading it here
# keeps the board showing the run the manifest actually predicted, instead of an
# empty table that quietly says nothing.
preds = []
register = os.path.join(ROOT, "APPENDIX-LOGS.md")
try:
    reg = open(register, encoding="utf-8").read()
except OSError:
    reg = ""
for m in re.finditer(r"^\| (P\d) \| (.+?) \| (.+?) \| (.+?) \|\s*$", reg, re.M):
    verdict = m.group(4).lower()
    # "void" is tested first: P2 reads "void, not a miss", and a void says the
    # method was wrong, which is not the same verdict as the number missing.
    if "void" in verdict:
        state = "void"
    elif "hit" in verdict:
        state = "hit"
    elif "miss" in verdict:
        state = "missed"
    else:
        state = "open"
    preds.append((m.group(1), m.group(2), state))
preds.sort(key=lambda p: int(p[0][1:]))

rules = []
ri = os.path.join(FILES, "RULES.index")
if os.path.exists(ri):
    rules = json.load(open(ri, encoding="utf-8"))["rules"]
counts = {}
for r in rules:
    counts[r["status"]] = counts.get(r["status"], 0) + 1

notes = []
for s in steps:
    for line in s["out"].split("\n"):
        if line.startswith("note  "):
            notes.append(line[6:].strip())

# Taken from MANIFEST §2, which is the single answer to "what is next"; a
# hardcoded run here went three rounds stale before anyone noticed.
console_item = {
    "title": "The run MANIFEST \u00a72 asks for",
    "body": (field("Next action") or field("Unblocked by") or "nothing queued")[:400],
    "done": False,
}

ok_all = all(s["ok"] for s in steps)
PASS = "#2ecc71"
FAIL = "#e74c3c"
AMBER = "#f39c12"


def pill(kind, text):
    color = {"ok": PASS, "bad": FAIL, "wait": AMBER}[kind]
    return '<span class="pill" style="background:%s">%s</span>' % (color, html.escape(text))


rows = []
for s in steps:
    detail = html.escape(s["out"][-1200:]) if not s["ok"] else ""
    rows.append(
        '<tr><td class="dot" style="color:%s">%s</td><td><b>%s</b><div class="what">%s</div>%s</td>'
        '<td class="t">%.1fs</td></tr>'
        % (PASS if s["ok"] else FAIL, "●" if s["ok"] else "●", html.escape(s["name"]),
           html.escape(s["what"]),
           ('<pre>%s</pre>' % detail) if detail else "", s["secs"]))

pred_rows = "".join(
    '<tr><td class="mono">%s</td><td>%s</td><td>%s</td></tr>'
    % (p[0], html.escape(plain(p[1])[:110]),
       pill({"open": "wait", "void": "wait"}.get(p[2], "ok"), p[2]))
    for p in preds)

note_html = "".join("<li>%s</li>" % html.escape(n) for n in notes) or "<li>none</li>"

page = """<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<title>Project status — PS4 media player</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
:root { color-scheme: dark }
body { margin:0; background:#0f1216; color:#e8edf2;
       font:15px/1.45 -apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif }
.wrap { max-width:980px; margin:0 auto; padding:28px 22px 60px }
h1 { font-size:22px; margin:0 0 4px }
.sub { color:#8b98a5; font-size:13px; margin-bottom:22px }
h2 { font-size:13px; letter-spacing:.08em; text-transform:uppercase;
     color:#8b98a5; margin:30px 0 10px; font-weight:600 }
.cards { display:grid; grid-template-columns:repeat(auto-fit,minmax(280px,1fr)); gap:12px }
.card { background:#151a21; border:1px solid #222a33; border-radius:10px; padding:14px 16px }
.card .k { color:#8b98a5; font-size:12px; text-transform:uppercase; letter-spacing:.06em }
.card .v { margin-top:6px; font-size:14px }
table { width:100%; border-collapse:collapse; background:#151a21;
        border:1px solid #222a33; border-radius:10px; overflow:hidden }
td, th { padding:12px 14px; border-bottom:1px solid #1d242c; text-align:left;
         vertical-align:top; font-size:14px }
tr:last-child td { border-bottom:none }
.dot { width:22px; font-size:15px }
.what { color:#8b98a5; font-size:12.5px; margin-top:2px }
.t { color:#6b7784; width:64px; text-align:right; font-variant-numeric:tabular-nums }
pre { margin:10px 0 0; padding:10px; background:#0b0e12; border:1px solid #2a1f1f;
      border-radius:7px; font-size:12px; color:#ffb4a8; overflow-x:auto; white-space:pre-wrap }
.pill { display:inline-block; padding:2px 9px; border-radius:999px; font-size:11.5px;
        font-weight:700; color:#0f1216 }
.mono { font-family:ui-monospace,SFMono-Regular,Menlo,monospace; color:#9fb0c0 }
ul { margin:0; padding-left:18px; color:#c4ced8 }
.foot { margin-top:34px; color:#6b7784; font-size:12px }
.big { font-size:15px; margin-top:10px }
</style></head><body><div class="wrap">
<h1>Project status — PS4 media player</h1>
<div class="sub">Generated __WHEN__ from the files themselves, by <span class="mono">tools/status.py</span>. Every check below runs on this machine; the console is listed separately because it cannot be reached from here.<br>Read order: this board, then MANIFEST §2, then AGENTS, then MANIFEST §9, then HISTORY. Nothing later overrides anything earlier.</div>
<div class="big">__OVERALL__</div>

<h2>Where we are</h2>
<div class="cards">
  <div class="card"><div class="k">Latest player build</div><div class="v mono">__PLAYER__</div>
    <div class="what">__PLAYER_NOTE__</div></div>
  <div class="card"><div class="k">Latest GPU package</div><div class="v mono">__GPU__</div>
    <div class="what">__GPU_NOTE__</div></div>
  <div class="card"><div class="k">Blocking question</div><div class="v">__QUESTION__</div></div>
  <div class="card"><div class="k">Next action</div><div class="v">__ACTION__</div></div>
</div>

<h2>Checks</h2>
<table>__ROWS__</table>

<h2>Waiting on the console</h2>
<table><tr><td class="dot" style="color:__WCOLOR__">●</td>
<td><b>__WTITLE__</b><div class="what">__WBODY__</div></td>
<td class="t">__WSTATE__</td></tr></table>

<h2>Predictions for that run</h2>
<table><tr><th>#</th><th>Claim</th><th>Result</th></tr>__PREDS__</table>

<h2>Notes</h2>
<ul>__NOTES__</ul>

<h2>Rule index</h2>
<div class="card"><div class="v">__NRULES__ rules indexed from the documents, with
status and evidence — <span class="mono">files/RULES.index</span></div>
<div class="what">__COUNTS__</div></div>

<div class="foot">Every row is produced by running the real checker, not by
remembering its result. Amber means the system is fine but a fact is unproven.</div>
</div></body></html>
"""

page = (page
        .replace("__WHEN__", time.strftime("%Y-%m-%d %H:%M"))
        .replace("__OVERALL__", pill("ok", "all host checks passing")
                 if ok_all else pill("bad", "a host check is failing"))
        .replace("__PLAYER__", html.escape(player))
        .replace("__PLAYER_NOTE__", html.escape(plain(field("Already run on the console"))[:120] + "…"))
        .replace("__GPU__", html.escape(plain(gpu)))
        .replace("__GPU_NOTE__", html.escape(plain(field("Built and not yet run"))))
        .replace("__QUESTION__", html.escape(plain(field("Blocking question"))))
        .replace("__ACTION__", html.escape(plain(field("Next action"))))
        .replace("__ROWS__", "".join(rows))
        .replace("__WCOLOR__", PASS if console_item["done"] else AMBER)
        .replace("__WTITLE__", html.escape(console_item["title"]))
        .replace("__WBODY__", html.escape(plain(console_item["body"])))
        .replace("__WSTATE__", "done" if console_item["done"] else "waiting")
        .replace("__PREDS__", pred_rows)
        .replace("__NOTES__", note_html)
        .replace("__NRULES__", str(len(rules)))
        .replace("__COUNTS__", html.escape(", ".join(
            "%s %d" % (k, v) for k, v in sorted(counts.items())))))

open(OUT, "w", encoding="utf-8").write(page)
print("wrote %s  (%d steps, %d passing, %d rules)"
      % (os.path.basename(OUT), len(steps), sum(1 for s in steps if s["ok"]), len(rules)))

if "--serve" in sys.argv:
    from http.server import BaseHTTPRequestHandler, HTTPServer

    class H(BaseHTTPRequestHandler):
        def do_GET(self):
            body = open(OUT, encoding="utf-8").read().encode("utf-8")
            self.send_response(200 if self.path in ("/", "/STATUS.html") else 404)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *a):
            pass

    print("serving on 0.0.0.0:3000")
    HTTPServer(("0.0.0.0", 3000), H).serve_forever()
