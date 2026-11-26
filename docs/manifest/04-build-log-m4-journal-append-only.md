# 4. Build log — **M4 · journal, append only**

*Part 4 of 12 of the manifest — previous: [Roadmap — **M3 · route and its statuses**](03-roadmap-m3-route-and-its-statuses.md) · next: [Decisions — **M5 · decisions and their rejections**](05-decisions-m5-decisions-and-their-rejections.md)*

Moved to `files/HISTORY.md` on 2026-11-24: the log had grown to 827 lines and was
crowding out the rules and the current state. The full entries — quoted log lines,
MD5s, source hashes, decisions and corrections — live there, verbatim and in date
order. This section is only the ledger, so a reader sees what happened without
loading the history.

| Date | Entry |
|---|---|
| 2026-11-23 | v113 (Liquid: water body, depth, framing) |
| 2026-11-23 | v114 (Liquid: glass, floor, spray, glint) |
| 2026-11-23 | v115 (the 467 ms freeze on Square) |
| 2026-11-23 | manifest: environment closed, GoldHEN and FTP researched |
| 2026-11-23 | v116 (glint, spray, floor — the three look tweaks) |
| 2026-11-23 | on hardware: the freeze, and Liquid's step |
| 2026-11-23 | manifest merge: the graphics standard becomes part of the goal |
| 2026-11-23 | manifest: GPU paths, console facts, techniques, and other players |
| 2026-11-23 | GPU path test: the piglet sample, built and packaged |
| 2026-11-23 | v117 (header overlap) and the video/audit analysis |
| 2026-11-23 | GPU test v2: a log written with kernel calls |
| 2026-11-23 | GPU test v3: the stage log (the one to run) |
| 2026-11-23 | the Piglet v3 log read: the GPU path is confirmed |
| 2026-11-23 | the v117 log read: nothing regressed, the header needs the screen |
| 2026-11-23 | manifest sync: graphics principles and the three rounds |
| 2026-11-23 | v118 (logging only) built |
| 2026-11-23 | Round 2 started, and stopped at the shader gate |
| 2026-11-23 | the shader catalogue package (BREW00094) built |
| 2026-11-23 | GNM checked, and the 60 fps skeleton written |
| 2026-11-23 | manifest: owner statements are folded in automatically |
| 2026-11-24 | the catalogue log read: a bloom chain CAN be assembled |
| 2026-11-24 | GPU Bloom POC built (BREW00095, no player version) |
| 2026-11-24 | the bloom log: the chain compiles, then dies at the first frame |
| 2026-11-24 | BLOOM 2 built (BREW00096): per-call error tagging |
| 2026-11-24 | the BLOOM 2 log: the chain runs, 85–100 fps, and the timings are not GPU timings |
| 2026-11-24 | BLOOM 3 built (BREW00097): sample draw out, GPU timings, real workload |
| 2026-11-24 | the BLOOM 3 log: my measurement is the bottleneck, and a uniform bug broke the blur |
| 2026-11-24 | other Piglet projects consulted, then BLOOM 4 built (BREW00098) |
| 2026-11-25 | buildstamp quoting fixed: the tree compiles again and two links of the same objects give a byte-identical `eboot.bin` |
| 2026-11-25 | the package is NOT byte-reproducible even when the binary is: two packagings gave different MD5s — measured, and it corrects the claim in `buildstamp.py`'s own docstring |
| 2026-11-25 | `make clean` repaired: it no longer removes `sce_sys/`, which held the only copy of `icon0.png` |
| 2026-11-25 | v119 built (R31 audio race: one mutex around the playback flags plus a track generation), host-tested, not yet run |
| 2026-11-25 | release body corrected on GitHub (FTP instead of a USB stick, MD5 added, "draft" removed) and [§2](02-current-focus-m2-state-the-one-block-to-read-first.md)'s Published row refreshed |
| 2026-11-25 | v119 run on the console: no errors, floor 21.4 ms; redesign charter written; four duplicate packages removed |
| 2026-11-25 | v124 built: the design-system pass (type ramp, spacing, radii, slab material, stage falloff) and the home screen rebuilt on it; icon rebuilt from the owner's own artwork inside a vault-door composition |
| 2026-11-25 | published manifest synced to the current state: [§2](02-current-focus-m2-state-the-one-block-to-read-first.md) (v119 run, v124 build, the pending v124 verdict), [§3](03-roadmap-m3-route-and-its-statuses.md) (v119 row, the redesign phases M0–M6), [§4](04-build-log-m4-journal-append-only.md) (ledger note) and [§11](11-next-steps-m11-plan-round-by-round.md) (Round 1 run, the redesign order) — commits `cee3164`, `55359d1`, `12cc79e`, `9a049de` on `main` |

[Manifest index](README.md) · [Back to the index](../README.md)
