# Plan

What happens next, as a work plan — not a rule. Nothing in it
overrides `AGENTS.md`.

[Back to the index](README.md).

---

# Plan (restored, full text)

Moved out of `AGENTS.md` on 2026-11-24 to bring the rules file under its G9
ceiling. The text is as it stood; the rules reference it as "the plan".

Round order: **Round 2 = flush fix + Stage B**, **Round 3 = Liquid look**, then
A3, A4, C, D, E.

**A3 — remove per-frame staging copies.** `rv_base()` copies the whole canvas
every frame and `art_present()` uploads the whole texture every frame, with a
full bilinear upscale at steps 2 and 3. Draw moving layers straight into the
base buffer where possible, and avoid copying the whole canvas when the changed
region is smaller.

**A4 — dirty regions.** Where a layer only changes in part of the frame (the DJ
booth, for example), redraw and upload only that region. Watch for stale-frame
artifacts and prove there are none.

**Stage B — soft things at low resolution.**
1. Render fog, glows, haze, light pools and bloom into a quarter-resolution
   buffer, upscale bilinearly, add onto the frame.
2. Cache fog/haze noise as a small tileable texture and scroll it.
3. Replace expensive blurs with pre-rendered glow sprites in a few sizes,
   stamped additively — lasers, disco ball, booth lights first.
4. Use lookup tables for gradients, falloff curves and color ramps.
5. Add a `config.ini` key switching full-res vs quarter-res soft layers so both
   can be compared on the console. Rebuild low-res buffers on canvas size
   change, quality step change, palette change and R3 reload. This changes the
   look slightly: render before and after and say honestly if anything is
   softer or blockier.

**Stage C — update less often.**
1. Refresh background crowd ranks and fog every second or third frame with
   interpolation; the front rank stays at full rate.
2. Update slow things (wall panel colors, light pool colors, haze drift) at a
   lower rate.
3. Skip work that is off-screen or fully covered (back-rank reflections hidden
   by the middle rank).
4. Tie it all to real time, not frame count.

**Stage D — cheap tricks that look expensive.** Only after earlier stages free
up time. Each gets a config key and a quality gate, off at step 3.
1. Edge color fringing (slight red/blue split toward the edges).
2. Laser lens flares (star and ring sprites at laser origins and the disco
   ball, scaled by brightness).
3. Bass-reactive color grade (gentle tint and contrast lift, never harsh).
4. Light shafts through haze (faint additive streaks from the DJ spotlight and
   disco ball, using the glow sprites).
5. Floor sparkle (tiny bright points on the glossy floor, twinkling with the
   highs).
6. Vignette breathing (tightens slightly on bass hits).
7. Dither before the vignette, plus nearly-free grain.
8. Depth cues (slightly desaturate and darken the back ranks).
9. Liquid: a faint specular glint sliding along crests, soft darkening at the
   tank bottom, tiny bubble sprites near breaking foam.

**Stage E — spend the freed time at step 2.** The console settles at step 2 or
lower, so that is the version actually seen. After the earlier stages, check
whether step 1 now fits the budget and whether the ladder stays at step 1. If
not, bring the best of step 1 into step 2: back-rank silhouettes, fog density,
rim light.

**Liquid look (Round 3).** Items 1-2 and the frame use in item 3 are done in
v113 (smoothed body light, green-to-blue depth ramp, water line up 8% with a
lit headroom). Still open: the glass' and the floor's own presence (item 3),
spray (item 4). Fix at the step the console actually runs (mostly 2):
1. Find where the vertical banding comes from (per-column crest value, the
   half-resolution canvas, or the lookup tables) and smooth it across columns.
2. Make the water read as depth: lighter and greener under crests, deeper blue
   toward the bottom, soft light shafts, caustics that visibly bend with the
   wave — cheap enough for step 2.
3. Give the front glass and floor more presence; use more of the frame for
   water, less empty dark room.
4. Slightly larger, more frequent spray when the waves are active.
5. Keep the crest rim and foam line. Keep the lighting in shared helpers so the
   other visualizers can reuse it.
