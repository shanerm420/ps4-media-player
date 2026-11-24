# Round-state notes carried over from AGENTS.md

Moved out of `MANIFEST.md` on 2026-11-24 under rule G9 (the manifest has a hard
128 KB ceiling and the rules must stay readable). Nothing here is a rule: it is
the round-by-round state history up to v118 and the GPU rounds. Read it when you
need the detail behind a §9 line.

## The notes as they stood

Moved out of the rules file to bring it under the G9 ceiling. These are
history and measurement; the present tense lives in the where-we-are block
at the top of §2. Nothing here was rewritten — the text is as it stood.

- Latest build: **v116** — `files/Ultimate-Media-Player-v116.pkg`,
  MD5 `7e215289a0f6ec58a55c50e4058e6b3c`. **On the console 2026-11-23** — the
  first run of this build: Square clean (worst paint 18.5 ms), Liquid at step 2,
  15 fps. The three look tweaks from the v114
  television photos: a glint with a bright core and a wide halo (+66 luma over
  567 pixels, all below the lip, against +52 over 262 in v114), spray that
  leaves the crests instead of lying across the tank, and a floor lifted 0.03 of
  the frame. Liquid only; NEON/WORM/RAVE DJ byte-identical to v114. Host bench
  26.74 / 15.08 / 11.36 ms at steps 1/2/3, and reverting any one tweak does not
  make it faster, so all three are free within noise. Notes in `files/round5/`.
- Previous: v115 — `files/Ultimate-Media-Player-v115.pkg`,
  MD5 `25f3af25c68a57fb8509de28fd83e40e`. The room background is built in
  slices, so a skin change (SQUARE on the main screen, the 467 ms freeze in
  three sessions' logs) no longer stops the screen. Not yet confirmed on
  hardware. Notes in `files/round4/`.
- Previous: v114 — `files/Ultimate-Media-Player-v114.pkg`,
  MD5 `b98a943f1c72f7a2582cb4e61475b985`.
  Round 3 finished: the tank's front glass and floor now have real presence,
  the spray is bigger and lasts longer, and five glints travel along the crests.
  The crest line and the foam code were not touched in v113 or v114. Most of
  the glass was missing on the console because it was gated behind a quality
  step the console never climbs back from; it now draws at every step.
  Notes and renders in `files/round3/`.
  Round 3 (first pass): Liquid's water body is lit by a smoothed copy of the
  surface instead of the per-column crest value (which saturated and made the
  flat cyan), the depth ramp runs green under a crest to deep blue with a
  longer-lived blue channel, and the water line rises 8% of the frame via the
  new `[liquid] surface_lift`, with two soft lamps in the headroom. Crest and
  foam untouched; NEON, WORM and RAVE DJ render byte-identical to v112. Notes
  and before/after sheets in `files/round3/`.
  **Hardware test (4.7 min, step 3):** Liquid runs 50.0-60.8 ms a frame
  (3-3.6 vsyncs) with paint 21.4-24.8 and flush 22.2, against 62.5-63.2 ms in
  v112 - so the new water costs no frames, and this session is slightly faster.
  The ladder dropped 1 -> 2 -> 3 on TESSERACT at 98 s (156.6 ms, 110.1 ms) and
  stayed at 3. Liquid was on screen full-screen at 152-155 s and 253-257 s, but
  no capture of it came back, so the *look* is still unconfirmed on the console.
  The 467 ms Square freeze on the music screen happened again (15.9 s).
- Previous: v113 — `files/Ultimate-Media-Player-v113.pkg`,
  MD5 `ce7c55f0749743620a86630792bc2449` (Liquid water body, depth, framing).
- Rollback before that: v112 — `files/Ultimate-Media-Player-v112.pkg`,
  MD5 `3714cd0a30921b72f226e877926fee4f` (ladder judges the whole frame,
  Stage B machinery behind `soft_quarter`, off; logs in `files/round2/`).
- Rollback before that: v111 — `73b67059b6ddfec44e9942520c472770`
  (log clock + screenshot capture), and v110 —
  `05fc21e97fea99b0d382ffe9be6762bd` (cached skins).
- Source snapshots in `files/archive/`: v104
  (`d69a49a195ffcd9808506a38fd47017e`), v110
  (`1a81ec3cef4ccfd0b653b74f3ee9db3c`, cited, not archived). Make a new one when asked.
- Measured facts from Round 3 (host, 1080p, 100 frames x3, Liquid): v113
  costs 25.94 / 13.93 / 10.88 ms at steps 1 / 2 / 3 against 22.19 / 12.05 /
  9.34 for v112, i.e. 10-17% more depending on the run (the host drifts 4-8%
  between sessions; the same v113 code measured 23.86 / 12.95 / 10.29 in an
  earlier session). That is inside the 20% ceiling; on the console it is about
  3 ms on a 62-66 ms frame. The cost is the two headroom lamps and the extra
  column pass.
- **The Liquid body must not be lit by the per-column crest value.** It is a
  height divided by 0.012 m, a swell here is about 4 mm, so it pins at 1 across
  the tank - that is exactly what made the water one flat cyan and drew the
  height field as vertical bands. Light the body from a smoothed height with a
  palette scaled to the swell (FL_BODY_M = 30% of crest_m) and keep the raw
  column value for the rim and the foam only.
- Widening the light shafts to soften them is expensive: a halo ellipse costs
  +2.0 ms at step 1 and +1.4 ms at step 3, a 25% wider core +1.8 ms at step 3,
  because a wide ellipse covers the whole canvas once the canvas is stepped
  down. Dimming the core is free.
- **A look element gated behind a quality step is a look element the console
  never shows.** The console settles on step 3 within seconds and does not climb
  back, so Liquid's side panes, the glow over the water and the pane highlights
  (all behind `s_paint_step <= 2`) were simply absent on hardware - that, and
  not the design, is why the glass read as a thin strip. Cheap elements belong
  at every step; at a stepped canvas they cost a few thousand pixels.
- **Bench an untouched look as a drift control.** The host ran 6-10% slower in
  the v114 session (Rave DJ, untouched, went 53.72 -> 57.36 ms at step 1), so
  raw before/after percentages from different sessions are not comparable.
  Liquid at step 1 reads +22% raw and +14% net of that drift.
- Round order still to come: A3, A4, Stage C, D, E.
- Measured facts from Round 2 (host, 1080p, 100 frames x3): the SDL flush costs
  a constant 3.6-4.1 ms at every step (it is one full-frame canvas copy into the
  24-bit window surface); the rasterizer's own draw is 5-50 ms depending on look
  and step; run-to-run spread is about +/-5%. Quarter-res soft layers and a
  separable canvas upscale both measured *slower* and are not in the build.
- **Step-3 appearance (host render of v112, `VIS_STEP=3`, 2026-11-23):**
  LIQUID and WORM stay close to full resolution; RAVE DJ and NEON visibly
  soften. Numbers are in `files/round2/step3-quality/metrics.txt`; the edge
  metric is dither-dominated on the smooth looks, so the images are the
  evidence. The console screenshot round trip is therefore optional, not
  blocking.
- **v112 on hardware (2026-11-23, 8 minutes of log, two sessions):** the
  ladder fix works — it dropped 1 -> 2 -> 3 with whole-frame reasons and the
  session then held step 3; Rave at step 3 is 66.7 ms (4 vsyncs) against the
  ~100 ms it used to take; the flush floor is 21.6-24.5 ms with a music list
  that draws nothing; every look is 62-82 ms. All 18 styles were visited. The
  user's `config.ini` predates v112 (`quality.soft_quarter` reported missing,
  default 0 in effect).

