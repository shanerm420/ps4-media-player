# P1 · Open problems, moved out of `AGENTS.md`

Moved here on 2026-11-26 because `AGENTS.md` sits at its 40 KB budget and its 640-line
ceiling, and this list is narrative rather than rule. Nothing was reworded: the items below
are the text that stood in the rules file, with their original numbers so any "open problem 7"
still means the same thing. `AGENTS.md` keeps a pointer and the rule that governs them.

**Upkeep:** an item is removed from this list only when a console run closes it, and the run
that closes it is named in the item. `(§9)` markers point at the manifest's knowledge base,
which is now `M9` with topics `K1-K13`.

---

## Known open problems

1. Visualizer frames sit at **62-82 ms (4-5 vsyncs, 12-16 fps)** at step 3 on
   the console - and the logged laps (M9 K15) put it on `flush`: 25.5-42.0 ms of canvas
   upload per frame, against 3-34 ms of paint and 8 ms of sim; the fast looks reach 50-60 ms (3-3.5 vsyncs). Rave improved to
   66.7 ms (4 vsyncs) from the ~100 ms (6 vsyncs) it ran at step 2 in v111. (§9)
2. The present path costs **21.6-24.5 ms on every screen**, even one that draws (§9)
   nothing: it blocks on a vblank (A7). The mechanism is now known (M9 K14): two buffers and a
   wait on every flip, a dead 8.29 MB memset per frame, a 3->4 byte row expansion, and no
   free scaling because the buffers are registered at panel size. Three fixes in payoff
   order: register at the drawn size (720p is stretched free, 2.25x fewer pixels), draw into
   the scan-out buffer, then use three buffers with a monotonic `flipArg`. Ceiling 60 fps, floor ~1.4 vsyncs (§9); a
   screen drops under 30 fps only when its own work exceeds a vsync.
3. NEON is the outlier: 116.8 ms (7 vsyncs) for a 2.5-minute stretch in the (§9)
   first session with `flush=72.2` (three times the copy floor), but 70.1 ms in (§9)
   the second session. Unexplained; needs one focused re-test.
4. The ladder did **not** climb back inside a look before v132, and the v138 log
   shows it never ran on a frame that drew no look: home sat at 1920x1080 with
   `flush` 133 ms and no `quality step` line. **Fixed in v139** (fed by every
   timed frame); unproven on hardware — that is what the v139 trip is for.
5. The pad opens as joystick only.
6. Music stall: 200–217 ms frames when the full-screen visualizer switches on,
   66–100 ms when a track starts. This is what "the music stall" means until
   the user says otherwise. The **467 ms freeze on Square** was not the library
   list: it is the room being re-baked for the next skin. The log shows paint
   435 ms with an unchanged flush on a screen whose paint was 0.9 ms a frame (§6),
   and the startup bake measures the same 441 ms. Fixed in v115 and **confirmed
   on hardware 2026-11-23**: forty-odd SQUARE presses, no frame over 50.1 ms, (§9)
   `worst_paint` 18.5 ms, `paint` 9.2–11.8 ms while a bake was in progress on (§9)
   every one of those frames.
7. Liquid: the flat cyan body, the vertical bands and the empty top third are
   addressed in v113 (see the Round 3 notes). Hardware numbers say the change is
   free (50-61 ms at step 3); what is *not* confirmed is how it looks on the
   television, because the console run was photographed on the Home and music
   screens only. v114 finishes the look: front glass, floor, spray and glint.
   Crest and foam line are good — they were not touched, keep them that way.
   Nothing named for this look is still open; what remains would be new work
   (a ripple on the glass, spray hitting the pane, reflected room detail).
   **On the console it settles at step 2, 15 fps** (66.7 ms a frame, paint (§9)
   33.4): the ladder stops there because step 2 is judged against
   `quality.budget_ms` (88 ms) and 66.7 is under it — it only drops to step 3
   below ~11 fps. Step 3 would give 20 fps on a third of the canvas;
   `config.ini` `[quality] budget_ms = 62` is the dial if smooth beats sharp.
   A default, not a defect.
8. Skins: v110 cached repeat visits, but the *first* visit still baked the
   whole room in one frame (~0.44 s). v115 builds it in ~12 ms slices while the
   outgoing skin keeps drawing, so a change costs no frozen frame and the new
   room arrives about a second later. Same pixels: checked on the host against
   a build forced to do the whole bake in one go.
9. v111's log clock and touchpad capture are unproven on hardware.
10. **Every look was invisible until v142, so the step-3 judgement this item used
    to carry was made from renders that held no look at all.** `s_tex` in
    `src/vis_art.c` was declared and never created, so `SDL_RenderCopy` was given
    NULL for the canvas and the picture never reached the screen. Fixed in v142
    and proved on the host: 0 bright pixels inside the look box before, 53,926
    after. The step-3 claim is withdrawn until looks are re-rendered and judged
    again; `VIS_STEP` itself is now measured working — all eighteen looks differ
    between steps 1, 2 and 3.
