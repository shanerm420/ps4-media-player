#!/usr/bin/env python3
"""bench-visuals.py - measure every rebuilt look on the host and write the bench line.

Writes files/bench/visualizers.txt, one line per look:

    style=3 name=PULSE ms=33.57 fps=29.8

Host numbers rank approaches; they are not console numbers (R6/R24). The point of
this file is that a rebuilt look cannot pass `make check` unmeasured.
"""
import csv
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "preview", "build", "preview")
REGISTER = os.path.join(ROOT, "docs", "visualizers.csv")
OUT = os.path.join(ROOT, "files", "bench", "visualizers.txt")
FRAMES = "40"


def main():
    if not os.path.exists(BIN):
        print("bench-visuals: SKIPPED - no preview build at preview/build/preview")
        print("               (run: make -C preview build/preview)")
        return 0
    if not os.path.exists(REGISTER):
        print("bench-visuals: FAIL - docs/visualizers.csv is missing")
        return 1

    env = dict(os.environ)
    env.setdefault("SDL_VIDEODRIVER", "dummy")
    # Pin the quality step. Without this the ladder steps down during the run and
    # the number is a mix of steps: 35.0 ms at step 1 and 17.9 ms for the same
    # look once it has fallen to step 3 both came out of the same command line.
    env["VIS_STEP"] = "1"
    home = os.path.expanduser("~")
    deps = os.path.join(home, "hostdeps", "root", "usr", "lib", "x86_64-linux-gnu")
    if os.path.isdir(deps):
        env["LD_LIBRARY_PATH"] = deps + ":" + os.path.join(deps, "pulseaudio")

    lines, failures = [], []
    with open(REGISTER, newline="") as f:
        rows = list(csv.DictReader(f))

    for row in rows:
        if (row.get("status") or "").strip() != "rebuilt":
            continue
        style = int(row["style"])
        name = row["name"].strip()
        # Best of three. This host is a shared VM and a single run drifts by up
        # to 15% depending on what else is running, which is enough to flip a
        # look over a 50 ms line without a line of code changing (SCOPE measured
        # 48.1 and 51.7 in two runs of the same build). The median of three is
        # the number that belongs in a gate.
        runs = []
        for _ in range(3):
            r = subprocess.run([BIN, "bench", "13", str(style), FRAMES],
                               cwd=os.path.join(ROOT, "preview"), env=env,
                               capture_output=True, text=True)
            m = re.search(r"([\d.]+) ms/frame \(([\d.]+) fps\)", r.stdout)
            if m:
                runs.append(float(m.group(1)))
        if not runs:
            failures.append("%s: no measurement in the bench output" % name)
            continue
        runs.sort()
        ms = runs[len(runs) // 2]
        fps = 1000.0 / ms if ms > 0 else 0.0
        lines.append("style=%d name=%-9s ms=%.2f fps=%.1f"
                     % (style, name, ms, fps))
        print("%-9s %6.2f ms  %5.1f fps" % (name, ms, fps))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write("# host bench, 1920x1080, mode 13, %s frames, this build, VIS_STEP=1 (pinned)\n" % FRAMES)
        f.write("# host numbers rank approaches; console numbers decide (R6/R24)\n")
        f.write("\n".join(lines) + "\n")

    for msg in failures:
        print("FAIL  " + msg)
    print("bench-visuals: %d looks measured -> files/bench/visualizers.txt"
          % len(lines))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
