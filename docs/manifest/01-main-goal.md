# 1. Main goal

*Part 1 of 12 of the manifest — previous: — · next: [Current focus](02-current-focus.md)*

Copied word for word from the project brief. Never edited unless the owner says so.

> You are the lead engineer and project keeper for a homebrew PS4 project. The
> main goal never changes: build a PKG that installs on a jailbroken PlayStation
> 4 and runs as a media player app. Beyond that app, you must collect and keep
> every piece of knowledge needed to build anything for the PS4, so future apps
> can be built from the same notes. Be smart and thorough. Never guess when you
> can verify. Never drift from the main goal when working on a small detail.
> After any detour, return to the goal and say where you are in the plan.

### The graphics standard — standing requirement, added by the owner 2026-11-23

This is part of the main goal now, not a preference, and it applies to **every
visualizer**, not only Liquid.

> Every visualizer must have the best graphics the PS4 can produce for what it
> is: modern real-time graphics quality, matched to the console's real
> capabilities. Native output resolution, steady frame rate, smooth motion, and
> effects such as bloom, glow, depth, particles, lighting and shader-based
> rendering wherever the hardware allows. No software-rendered, low-resolution
> or low-frame-rate result if a better method exists on this hardware. Any
> quality trade-off is logged as a decision with its reason and revisited later.

How it has to be carried out, as instructed:

- Research what the PS4 GPU can do for each visualizer topic, **using five
  different phrasings per topic**, and record the findings and every source
  disagreement in the knowledge base ([§9](09-ps4-knowledge-base.md), sources in [§8](08-resources.md)).
- **Audit every existing visualizer** against the standard and log where each
  falls short (defects in [§6](06-problems-and-fixes.md), the work in [§3](03-roadmap.md)).
- Fix the biggest gaps first.
- A **software-rendered, low-resolution or low-frame-rate result is not
  acceptable** if a better method exists on this hardware.
- Any quality trade-off is logged as a decision ([§5](05-decisions.md)) with its reason and the
  alternatives rejected, and revisited later. A trade-off is not a settlement.
- The owner expects this to mean **moving rendering onto the GPU**. The audit
  confirms or rejects that, and the roadmap carries it as steps ([§3](03-roadmap.md)).
- How every graphics decision is made from now on — the research/measure/rule
  loop, the rules already measured on this console, and the approaches already
  rejected — is [§9](09-ps4-knowledge-base.md) **Graphics principles**. Those rules are the same standing
  requirement in practice: they outrank tuning, taste and the old software
  canvas.

**Why this matters here, in the owner's words:** Liquid currently runs at 15 fps
on a reduced canvas, which points to CPU rendering. That is the key gap against
this standard — not a detail, and not a default to defend. Every later decision
about the visualizers is measured against it.

[Manifest index](README.md) · [Back to the index](../README.md)
