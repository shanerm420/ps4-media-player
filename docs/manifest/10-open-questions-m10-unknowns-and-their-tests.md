# 10. Open questions — **M10 · unknowns and their tests**

*Part 10 of 12 of the manifest — previous: [PS4 knowledge base — **M9 · knowledge: what this console does**](09-ps4-knowledge-base-m9-knowledge-what-this-console-does.md) · next: [Next steps — **M11 · plan, round by round**](11-next-steps-m11-plan-round-by-round.md)*

**Answered 2026-11-23 — the first GPU log was empty because the redirect failed,
not the disk.** v3's first line, written before any module loads, arrived from
`/data/piglet-gpu-test-v3.log`, and two more sinks opened beside it ([§9](09-ps4-knowledge-base-m9-knowledge-what-this-console-does.md)). The
filesystem was writable all along; what produced nothing in v1 was
`freopen`/stdio on stdout. Kept open only in the narrow form: whether that is
`freopen` itself or the stdio buffer, and whether it would ever have flushed.
No further test is planned — the kernel-call writer works and is the pattern to
reuse.

1. ~~Is the freeze actually gone on hardware?~~ **Answered 2026-11-23**: yes.
   42 presses, no frame over 50.1 ms, worst paint 18.5 ms.
1a. Is 15 fps right for Liquid on this console? The ladder stops at step 2 because
   its budget is 88 ms; step 3 would give 20 fps on a third of the canvas. Plan:
   the owner tries `[quality] budget_ms = 62` once, and says which they prefer —
   smoothness or sharpness. Nothing to decide from here.
2. Do the darker patches under the water's dips on the television belong to the
   rendering or to the camera? Plan: still open — v116 did not touch the body's
   vertical shading, and a capture of the same moment would settle it.
3. Do the v116 glint and the crest-clustered spray read on a television? Plan:
   the next capture. The glint is now measured at +66 luma over 567 pixels on
   the host, which is the most it can be argued from here.
4. The NEON outlier (116.8 ms for a stretch in one session, 70.1 ms in another)
   is still unexplained. Plan: one focused re-test with the log running.
5. Whether the 12.52 "Testkit" support GoldHEN v2.4b18.7 mentions changes
   anything for a retail console. Plan: none needed unless something breaks;
   noted so it is not rediscovered as a surprise.
6. Which looks can be GPU-rendered without rebuilding the chrome? Plan: the
   audit settles it, starting from the fact that a GNM driver session already
   exists while the window and its flip belong to SDL.
7. Why video plays at 16 fps with 15,397 pull failures. Plan: a run at
   `log level=2` on the same file, then read the video notes before changing
   anything.
8. What the right frame rate for Liquid is under [§1](01-main-goal-m1-the-goal-verbatim.md) — 15 fps at step 2 is not
   defensible, 20 fps at a third of the canvas is a trade-off, and full
   resolution at 30 fps may need the GPU. Plan: the audit prices all three.
9. **Does homebrew on this Pro run in Pro mode or base mode?** 36 CUs at ~4.2
   TFLOPS or 18 at 1.84 changes every budget. Plan: measure or query, do not
   assume; log the method with the answer.
10. ~~Do GLES2/Piglet apps need `libScePigletv2VSH.sprx` and `libSceShaccVSH.sprx`
    on 12.52?~~ **Answered 2026-11-23**: `libScePigletv2VSH.sprx` and
    `libScePrecompiledShaders.sprx` load from the firmware with
    `module_start() = 0`; `libSceShaccVSH.sprx` was never requested and is not
    needed by this sample ([§9](09-ps4-knowledge-base-m9-knowledge-what-this-console-does.md)).
11. What licence does projectM's preset corpus carry, and can a warp-feedback
    effect be written cleanly from scratch instead? Plan: check before any code;
    from-scratch is the default.
12. ~~Do the system Piglet modules exist and load on 12.52 retail?~~ **Answered
    2026-11-23**: yes — handles 21 and 22, both `module_start() = 0`, on 12.52
    with GoldHEN v2.4b18.14.
12a. **What does a visualizer actually cost on the GPU?** Nothing is drawn
    through Piglet yet, so there is no number. Plan: the proof of concept — one
    look, half-float render targets, a bloom pass, at 1080p — measured in
    milliseconds and could-be-vsyncs, then decided. No port before that number
    exists.
12b. **Can SDL's window and flip survive a second presenter?** Piglet renders
    to its own surface and swaps it; the player's window belongs to SDL. Whether
    the two can interleave, or the GPU look has to present the whole frame, is
    the first thing the proof of concept has to settle.
13. Is OpenOrbis **0.5.4** real, and does it change the piglet headers or stubs?
    `shipofharkinian-ps4` mentions v0.5.4; our release zip is 0.5.2. Plan: check
    the releases page before any port work, so the test is not run on the oldest
    GL headers in the line.

[Manifest index](README.md) · [Back to the index](../README.md)
