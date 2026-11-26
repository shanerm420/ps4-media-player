# 2. Current focus — **M2 · state: the one block to read first**

*Part 2 of 12 of the manifest — previous: [Main goal — **M1 · the goal, verbatim**](01-main-goal-m1-the-goal-verbatim.md) · next: [Roadmap — **M3 · route and its statuses**](03-roadmap-m3-route-and-its-statuses.md)*

### Where we are — the one block to read first (2026-11-25)

The single answer to "where is this project now". Updated on every build, package
and log read, never left to the end of a round. Prose history lives in [§4](04-build-log-m4-journal-append-only.md) and the
knowledge in [§9](09-ps4-knowledge-base-m9-knowledge-what-this-console-does.md); this block is only ever the present. Process rules live in
`AGENTS.md` (G1–G13); **G12** says a build unknown is not a "no" until every
means has been tried, and unconventional methods are named before use.

| Field | State |
|---|---|
| Latest player build | **v144** — `files/Ultimate-Media-Player-v144.pkg`, 6,619,136 bytes, MD5 `2c7a3a6f070c53d9ce47555ac2670795`, build id **`v1.44-8c6bccde0f11`** (rollback: **v138**, `7d3fafb8ec89702696900346c0e99330`; v140-v143 archived unrun) — built 2026-11-25 with `make VIS_PLATE_FX=1`, 17/17. Adds the compatibility layer (`src/compat.c`): a boot probe that loads AvPlayer, resolves its frame getters by name and logs what the firmware answered, with a fallback path when an export is absent. Docs: `docs/COMPATIBILITY-LAYERS.md`. **Built, not run.** |
| Latest GPU package | **BREW00103** — `files/GPU-Bloom-Slope-Probe-BREW00103.pkg`, MD5 `3bde518b33bf17627d5ab559b84f0084` — GPU-clock slope per A6/R24, `CLEAN` line now reported, one log path with automatic cleanup, checked by hand with `tools/check-pkg.py` (the build did not call it then), **built 2026-11-24, not yet run** |
| Built and not yet run | v144, v143, v142, v141 and v140 (v138 ran 2026-11-25 23:09:05); BREW00103 |
| Already run on the console | Piglet sample v3 (GPU confirmed), BREW00094 catalogue (82 blobs), BREW00095 (502 at first frame), BREW00096 (85–100 fps, submission timings only), BREW00097 (15 fps; `glFinish` and vec4 bugs, see [§9](09-ps4-knowledge-base-m9-knowledge-what-this-console-does.md) A1/A2), BREW00098 (8.19 / 8.32 ms = the present floor, not the work; C died on a uniform type), BREW00099 (empty 16.68 ms = one vsync; n1/n4/n16 all VOID on `0x0502`), BREW00101 (five configs `ok`, every `SET` refused: program never bound), BREW00102 (installed after the package fix; **all 23 `SET` accepted**; panel legible; bloom visible; 16.656 / 16.654 / 16.658 / 16.659 / 33.340 ms for N = 0/1/4/16/64 = 1/1/1/1/2 refreshes; **v119** 2026-11-25: 2,921 tracks, ladder 1→2→3 (110.8 / 94.7 ms), floor 21.39–21.56 ms, no errors) |
| Blocking question | **Has any visualizer ever been seen?** Nothing in the record proves it: the canvas was drawn and never uploaded, and every host render of a look was the music screen. From v142 the texture is created and the host shows all eighteen looks at all three steps. What is left is the console: the home frame (`flush` 133 ms → step 2), the quick menu, and the first sight of a look on a television |
| Unblocked by | nothing: the **v119** run happened 2026-11-25 02:41:26. The redesign order is `REDESIGN-CHARTER-2026-11-25.md` [§7](07-environment-m7-machine-toolchain-commands.md). BREW00103 stays built and unrun on purpose: it would tighten the bloom figure from "under half a millisecond" to a decimal, and no decision needs that (R27) |
| Next action | **Install and run v144** — `files/Ultimate-Media-Player-v144.pkg`, MD5 `2c7a3a6f070c53d9ce47555ac2670795`; sheet `docs/CONSOLE-TRIP-2026-11-25.md`. Pull `app.log`: it must name `v1.44-8c6bccde0f11` and print the `compat:` block (`avplayer module handle=N`, `GetVideoDataEx=yes`, four `rc=` lines), plus `screen: home`, `quality step 1 -> 2` and a picture in the music screen's box |
| Reflection (R26/R27) | **Already recorded:** the 16.7 ms step, the uniform binding, vsync, packaging, logs, module load, memory class, blob set ([§7](07-environment-m7-machine-toolchain-commands.md)). **Genuinely new:** the file's codec, rate, duration, bitrate, and whether `??????` names arrive mangled. Both need this console |
| Last console-verified | **2026-11-25, v138** (player, run 23:09:05 — home screen only; the pad stopped answering there); 2026-11-24, the BREW00102 log (GPU) |
| Published | `shanerm420/ps4-media-player` — source, docs, `AGENTS.md`, tools and notes on `main` at `b445e68` (merges `295f9db`, then `fa4f556`, then PR #3); release `v1.0.0` **published** 2026-11-24T23:48:23Z with asset `IV0000-BREM00001_00-ULTIMATEMEDIA000.pkg`, 6,619,136 bytes, MD5 `fb2189115fdb683cd52b3148d77562c2`, SHA-256 `482d214c92c66784107675eed09b759f88c7a97ef7e8f294766e4dcc80530f59`, both read from the asset and confirmed here against the archived copy of the same bytes; CI `links` green on `b445e68`. **Provenance:** tag `V1.0.0` (`78ac0bb`, 21 Nov) is the build source of the shipped asset; tag `v1.0.0` points at the docs merge and is the release's tag, so its source zip is not the source of that binary. `main` also still carries the unverified A3/A3b renderer change, which this tree does not — the two sources differ |

**Who decides.** One acting agent — the one talking to the owner — owns the translation from intent to build: it sets the design, judges every claim, including the claims other AI tools return, decides what is right or wrong against the owner's recorded requirements, and signs the package. Other AIs are instruments, not authors: their output is tested before it changes a file. Rule G11; full text in `docs/AGENT-AUTHORITY.md`.

**Focus: the frame the home screen actually gets.** The v138 log is the first
one that measured a screen nobody was looking at as art: 133 ms of `flush`, 1.7 ms
of `paint`, and a ladder that never judged a frame it did not draw. v139 is the
answer to that, and the next log decides whether it worked. The GPU path stays
where the bloom probes left it — reachable, measured, and not needed for this.

**v141 is the player build and has not run.** v138 ran on 2026-11-25 23:09:05 and
gave the run that rewrote the frame budget: `v137` is the rollback before it. The
header fit landed in v117 and has still not been judged on the screen. The
`??????` filenames, the settings screen, the search index and the visualizer
overhaul are still queued and untouched.

**Video: the 16 fps is not decoding.** This player does not use FFmpeg — video
goes through Sony's `libSceAvPlayer`. During playback the log reads `paint=0.9`,
`flush=35–39`, `present=6–19`, `conv_avg=5.91` against `avg=50–67 ms`, so the
frame is spent in the presentation path (video blit plus the full-frame copy),
not in the decoder. The `15,397 pull failures` are **empty polls, not failures**:
the drain loop adds one on the first call that returns no picture and breaks, so
every productive drain also adds one. The counter needs `pull_rounds` (already
counted, never printed) before it can be read at all. What the log cannot say is
the file's codec, nominal frame rate, duration and bitrate — that logging is the
next step, and no player source has been touched.

**Audit: nothing fits.** All eighteen looks were timed on the host: the fastest
(LIQUID, 27.8 ms at step 1) is 1.7× over a 60 Hz frame and WORM is 4.5× over, so
native resolution is unreachable by tuning — which is why the console lives at
step 2/3, 15–20 fps at half or a third of the canvas. No look has a shader, a
real bloom pass or a lighting pass. Ranked gaps and the two-target plan are in
`files/round7/`.

**Header: fixed in v117, and v117 ran.** The title and status lines measure
themselves against the emblem's edge and end in `...` rather than running
underneath it. The v117 log (83.9 s, all eighteen looks toured with Triangle)
has no `long frame` after 7.2 s and no press over 66.8 ms, so nothing regressed;
whether the `...` appears is still only answerable on the screen.

**Pending from the owner — recorded as not reported, nothing assumed:**

- The **v124** home screen: resolution, crispness, the type ramp at 10 feet,
  whether the five-door home reads without instructions. **Not reported.**
- What the Piglet sample showed on the television: **not reported.** The log
  proves frames were presented (`STAGE 9`, 3,300 of them) and says nothing about
  what was visible.
- The v117 long-title header, whether the line ends in `...` before the emblem:
  **not reported.**
- The v116 look verdict — glint, spray, floor: **not given yet.**

The console steps in [§11](11-next-steps-m11-plan-round-by-round.md) carry the exact things to look at, so these arrive with
run reports rather than as questions.

**The logging build went to the console as v119, and the lines it exists for did
not appear.** Logging only — no player logic changed: `video: SRC` (brand, codec fourcc, bytes, resolution, duration,
bitrate), `video: RATE` (the file's own rate from 30 frame timestamps),
`pull_rounds` and `rounds_per_frame` in the `FILE` line and in the periodic
summary, `Stop`/`Close`/teardown/open timings, and one `library: name` line per
non-ASCII filename with its bytes, its UTF-8 validity and the atlas verdict.
Built as v118 (`349943cbd65ddb1580a9e47aec7de1b0`) and run as v119 on 2026-11-25:
no errors, 2,921 tracks, floor 21.39–21.56 ms, and no `video: SRC` / `video: RATE`
/ `pull_rounds` line in the log (E10), so the file's codec, rate, duration and
bitrate are still unknown. The GPU proof of concept is done (BREW00102/103); the
look port is not, and no visualizer work starts before the owner approves it.

Why this matters to the main goal: the video answer shows the same wall as the
visualizers did — the present path, not the codec — and the audit turns the
graphics standard into a ranked list for the first time.

[Manifest index](README.md) · [Back to the index](../README.md)
