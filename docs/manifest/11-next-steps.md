# 11. Next steps

*Part 11 of 12 of the manifest — previous: [Open questions](10-open-questions.md) · next: [PS4 development standard (merged 2026-11-24)](12-ps4-development-standard-merged-2026-11-24.md)*

**Standing reply rule (added 2026-11-23, owner's instruction):** every reply ends
with the **Logical next step** — the single decision or action that comes next,
plus the exact information needed to make it: the file, package, version or
number involved, what each branch leads to, and what to look at or send back. It
is a proposal, not a question, and it never leaves the owner to reconstruct
context. When nothing is left to decide, the reply says so and names the step
after that. This replaces the older "end on a question" and "never end on a
question" habits: the ending is now always a decision the owner can actually
make.

**The Logical next step must also carry its instructions (added 2026-11-23,
owner's instruction).** Naming the step is not enough: the same paragraph states
how to execute it, in order, as far as it can be written down — the exact file
and MD5 to FTP, the package source to install from, how long to run it, the exact
log path to pull and in which mode, and the specific lines to send back. For a
build step that means the command, the package name, the MD5 and the source hash.
The test is simple: the owner should be able to act on the last paragraph without
asking a follow-up question, and should know exactly what to look for. If the step
is the owner's and it needs something they have not given yet, the three
unreported observations in [§2](02-current-focus.md) are the model for how to ask for it.

**Owner-supplied information is folded in automatically (added 2026-11-23,
owner's instruction).** When the owner states something that bears on the build or
on how this console, this toolchain or this kind of software behaves, it goes into
the right section without a separate request — a fact into [§9](09-ps4-knowledge-base.md), a console behaviour
into [§7](07-environment.md), a rejection into [§5](05-decisions.md), a failure into [§6](06-problems-and-fixes.md), a number into the Graphics
principles table with its status attached. The filter is usefulness to the work,
not length or politeness: it must help this build, or the understanding of the
system this build sits on, or a future project that inherits these notes. Recorded
as stated, with the date and who said it; contradictions get both readings and a
note to test rather than a silent overwrite. **Do not** record passing preferences,
anything already covered, or a remark whose only use is conversational — a
manifest that fills with noise stops being read, which is worse than one with a gap.
When it is unclear whether a statement is durable, record it with its status
(`owner-stated, untested`) instead of leaving it out. This is how the file stays
worth reading after this project ends: the goal is a knowledge base future apps
can be built from, not a diary of the build.

**Asking is allowed when it is the instruction (added 2026-11-23).** The owner may
ask to be consulted, and then a reply ends with the question instead of a
proposal: state the options, what each one costs and what each one unlocks, and
wait. "Do not end on a question" applies to unprompted endings, never to a
request for a decision.

**Continuous progress rule (added 2026-11-23, owner's instruction):** keep moving
to the next logical step without waiting for a new approval, as long as that step
is inside the plan already agreed. Do **not** stop between rounds to ask "shall I
start?" — a round that is already written down is already authorized, and its
build, its tests, its package, its log entry and its backup are part of finishing
it. Stop and ask what the next move is only when new information is genuinely
needed from the owner: a console observation, a run or a pulled log, a choice
between measured options, a decision the plan explicitly reserves to the owner, or
a permission the hard rules gate (deleting or moving files, publishing, pushing,
changing something the request did not name, porting a visualizer before the
proof of concept is approved). When stopping, name exactly what is needed, why the
work cannot continue without it, and what each answer would unlock — and every
other rule still applies while doing it: one change at a time, measure before
claiming, verified/assumed/unknown, build log with MD5 and source hash, rollback,
milestone backup.

**Look for the existing solution before asking.** When a problem turns up that
other people have already met — a build error, a missing symbol, a driver
refusal, a libc oddity, a console behaviour, a package that will not install —
find out how they solved it before asking the owner for anything. Search several
phrasings as the research rule says, read the toolchain's own samples, headers
and sample code first, then ps4libdoc, the PS4 Developer Wiki and open-source
homebrew that uses the same library, and record the fix with its source the way
every other researched fact is recorded. This is also how the queue of known
PS4 problems is meant to be used: `files/bootstrap-toolchain.sh`, the toolchain
samples and the earlier rounds' logs already contain the answers to build and
environment questions, and re-deriving them wastes the owner's time. The owner
is asked only when the answer needs something no source can supply — a fact only
this console, this account or this owner has, a run or a pulled log, or a choice
between measured options the sources cannot decide. A failed build is not a
question: an error another project already worked around is a lead to follow, not
a wall to report.

**New evidence amends the rule, and the amendment is recorded (added
2026-11-24).** When a measurement, log line or source contradicts an existing
rule, the rule itself is edited in place — not merely contradicted somewhere
else — in the same turn, before anything is built on the old version. Each
amendment carries the date, what the rule said before, what it says now, and the
evidence that forced it; the previous wording moves to a one-line "superseded"
note ([§6](06-problems-and-fixes.md) or the build log) so nothing is lost and nothing is silently rewritten.
**An amendment may make a rule stricter on its own; it may never weaken a rule
that protects the owner's files, data, legality or build stability without the
owner agreeing first.** If the evidence suggests a change but does not prove it,
both readings stay, marked `contested`, with the test that would settle them.
Amendments themselves are listed in [§9](09-ps4-knowledge-base.md) to keep them auditable.

**Where we are is updated on every event, not every round (added 2026-11-24).**
The block at the top of [§2](02-current-focus.md) is the single answer to "where is this project now".
It is edited in the same turn as any build, package, console run or log read —
rounds are where drift hides — and it names the latest player build and its MD5,
the latest GPU package and its MD5, everything built but not yet run, what has
already run on the console, the one blocking question, who unblocks it, the exact
next action, and the date of the last console-verified result. It carries no
prose and no history: those belong in [§4](04-build-log.md) and [§9](09-ps4-knowledge-base.md). `tools/check-manifest.py` fails
if the block names an older player or GPU package than the newest one in
`files/`, so the summary cannot rot silently.

**Existing work is a starting point, not a tutorial (added 2026-11-24).** If a
project has already solved the problem, use their answer — or the approach their
code takes — rather than re-deriving it, and say which project's solution was
used. Writing it fresh is allowed only when nothing suitable exists, nothing
licence-clean exists, or their solution demonstrably fails on this console, and
then the reason is written down before the work starts. This is not only about
asking the owner for help: it is about not spending a round — and a console run
— rebuilding what somebody else already has working. The check is cheap and the
record is one line in [§8](08-resources.md); the cost of skipping it has already been paid twice
(the vec2 blur radius and the per-pass `glFinish` trap, both discoverable from
other homebrew before the first package was built).

The three rounds below are **authorized as written**. Under the continuous
progress rule (above) they are worked through in order without a fresh "shall I
start": Round 1 begins immediately, and Round 2 begins as soon as Round 1's
package, tests, log entry, backup and console steps exist. The stops that remain
are the ones the rules create — the owner's console run, the owner's verdicts, the
approval before any visualizer is ported, and anything the hard rules gate.

Synced 2026-11-23 to the owner's consolidated instruction. Three rounds, one at
a time, each finished and logged before the next starts. The queued features keep
their place after them, and nothing renders differently until Round 2 is
approved.

### Predictions register (added 2026-11-24)

Before a console run, the expected number is written here — with what would
falsify it — so the log confirms or refutes a written claim instead of being
narrated afterwards. A prediction that misses is more useful than one that hits:
it says the model is wrong while the evidence is still fresh. Verdicts go in the
`Result` column in the same turn the log is read, and a miss becomes an
amendment (A-number) if it moves a rule.

**BREW00098 (five configs, 120 frames each, whole-frame timing).**

| # | Claim | Falsified if | Result |
|---|---|---|---|
| P1 | `A empty` (clear + swap) lands at **8–10 ms**, because the swap alone measured 8.3 ms in the v3 sample and nothing else draws | under 6 ms (the swap is not what we think) or over 14 ms (something else is pacing) | **hit** — 8.19 ms for 120 frames |
| P2 | `B−A` (ramp + 2,000 sprites at 1080p into the half-float target) is **4–8 ms** | over 14 ms (a 1080p fill that heavy would make a 60 fps look impossible on this path) | **void, not a miss** — 0.13 ms, because both A and B sat on the 8.3 ms present floor; the method, not the model, was wrong ([§6](06-problems-and-fixes.md)) |
| P3 | `C−B` (bright, blur1, blur2, composite at 480x270) is **1–3 ms** | over 6 ms (bloom is then too expensive to use everywhere) | **missed, on the cheap side** — BREW00102 bounds one chain under ~0.5 ms by the CPU window; the estimate was 2-6x too pessimistic, and the GPU-clock run (BREW00103) is what would replace the bound with a figure |
| P4 | `D−C` (4,000 extra sprites) is **1–3 ms**, i.e. sprites scale roughly linearly | flat 0 ms (the sprite draw is not the cost) or over 6 ms (vertex-bound already) | not run |
| P5 | `E−C` (960x540 bloom targets instead of 480x270) costs **+1–3 ms**, i.e. bloom is fill-bound at its target size | flat 0 ms (bloom is fixed-overhead bound, which changes the tuning plan) | not run |
| P0 | **All five configs report `err=0x0000`** after the vec2 fix (A2) | any non-zero error voids that config's number and the run | **MISSED** — `[PIG]E-glUniform2f:8390 - illegal to set uniform data with different types`, then `glViewport failed: 502` |

Rule of the register: **no number from this run enters [§9](09-ps4-knowledge-base.md) until its row is
filled in**, and any config whose error is not zero is reported as void rather
than quietly dropped.

### Round 1 — v118, logging only — **built, awaiting the console run**

Built 2026-11-23, MD5 `349943cbd65ddb1580a9e47aec7de1b0`, source hashes in [§4](04-build-log.md).
The console steps and what to look for are in the round report; v117 is the
rollback. Nothing in Round 2 may start from assumptions about this log.

- At video open print the codec or fourcc, the nominal frame rate, the duration,
  the bitrate and the resolution; add `pull_rounds` to the video summary beside
  `pull_ok`/`pull_fail`.
- For a `??????` filename print the raw bytes, the decoded result, and whether
  each character has a glyph in the atlas (`src/font_atlas.c` carries ASCII
  32–126 only), then state the cause. **Do not fix it yet.**
- Print the open and teardown stage timings, which is where the 383.8 ms and
  250.2 ms frames land.
- **No player logic changes.** Built as **v118** with its own MD5 and a source
  hash, `make check` plus the preview build, and **v117 stays the rollback**.
- Console steps go in the round report: which file to open, what to play, how
  long to wait, which log to pull, and what to look for in it.

### Round 2 — GPU proof of concept — **effect proven 2026-11-24, cost bounded**

Status, before the plan text below: the bloom chain was built and run in its own
packages (BREW00094 to BREW00103), it draws on the console, and the numbers say
one chain is under about 0.5 ms by the CPU bound - a six-pass chain at most ~3 ms
of a 16.7 ms frame (R24/A6, [§9](09-ps4-knowledge-base.md) A6). What is **not** proven is the look port and
SDL/Piglet surface sharing, which Round 3 needs. The bullets below are kept as the
plan that produced it, with one correction: the logging rule is now R22 - **one**
sink in `/data/Alpha PS4 test kit/`, not one at `/data` and one in the player's
folder.

- A **separate package with its own title id**, built from the OpenOrbis 0.5.2
  Piglet sample. It never replaces the player, changes no player source, and
  uses **no player version number** — v118 stays the player's next number.
- Keep the stage-numbered, fsynced logging that worked in v3: `sceKernelOpen` /
  `sceKernelWrite` / `sceKernelFsync`, one sink at `/data` and one at
  `/data/ultimate-ps4-media-player/`, heartbeats and per-frame timings.
- **One look, the one with the biggest visible gain**, with the reason written
  down. 1920x1080 through Piglet on half-float render targets, a bloom chain
  (scene, bright-pass to a smaller target, blur, composite), ping-pong trails
  where the look gains from them, and real per-band audio analysis — log-spaced
  bands, per-band beats with a refractory interval and geometric decay, packet
  interpolation, automatic gain.
- Frame times in every summary: the **GPU timer query if it works, CPU frame
  time otherwise, and every number labelled with which one it is**.
- The answer is measurements: cost at 1080p, whether it holds 60 fps, and the
  worst frame.
- Install, run, close and log-pull steps, ending with "look at this on the
  television and report it": resolution, smoothness, glow, anything wrong.

### Round 3 — plans only, after Round 2 is logged

- **SDL and Piglet handover.** Options — the visualizer as a full-screen mode
  that hands the display over and back, the whole interface drawn through
  Piglet, or a hybrid — each with its cost, its risk, what Round 2 measured
  about it, and the test that would settle it. One recommendation with reasons;
  where Round 2 already shows sharing works or fails, that evidence is used and
  said so.
- **A 60 fps budget for the visualizers.** From the measured Round 2 numbers, a
  16.7 ms budget split across audio analysis, scene, bloom and composite; for
  each of the eighteen looks whether it can hold 60 fps on the GPU, what it must
  drop or simplify if not, and which need the most work. Plus a quality fallback
  that steps a look down by itself, and what each step trades away.
- Both written into `files/round7/README.md` and this manifest. **Nothing built.**

The three items the owner still owes — what the sample showed on the television,
the v117 header, the v116 verdict — are recorded as unreported in [§2](02-current-focus.md), and no
round below depends on them.

### After the three rounds

4. **Filenames and languages** — font coverage and the UTF-8 path, once Round 1
   has said whether the name arrives mangled or the atlas lacks the glyphs. Needs a
   glyph set well beyond ASCII 32–126, which is all `src/font_atlas.c` carries
   today.
5. **Settings screen** — the natural home for `[quality] budget_ms`, the quality
   steps and whatever knobs the proof of concept adds; worth building after that
   proof, not before.
6. **Search and library index with metadata tags** — the biggest of the queued
   features and the one that depends on how the library is keyed, so it follows
   settings rather than leading.
7. **The visualizer overhaul of all eighteen looks, from the owner's feedback** —
   last, and only after the proof of concept is approved. Nothing is rewritten
   before that: the audit's order (WORM, RAVE DJ, WINDMILL, the DANCE family,
   then the cheap spectrum looks) is a proposal, not a start.
8. Then A3 and A4 on whatever still runs on the software canvas, and Stage C–E

[Manifest index](README.md) · [Back to the index](../README.md)
