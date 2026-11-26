# PS4 media player — project knowledge

A native PS4 home media player built with the OpenOrbis toolchain, and the
documented system that keeps its engineering honest: rules with statuses,
measurements with provenance, a dated log, and checkers that fail when the
paperwork stops being true.

## If you are an AI agent, read in this order

1. [AGENTS.md](../AGENTS.md) — the rules. Hard rules, workspace rules,
   measurement rules, and G1–G10.
2. [Manifest §2](manifest/02-current-focus-m2-state-the-one-block-to-read-first.md) — where the project is
   right now: latest builds, the blocking question, the next action.
3. [Rule index](rules/README.md) — every rule with its status and the
   evidence behind it. `measured` means a console log proves it.
4. [Manifest §9](manifest/09-ps4-knowledge-base-m9-knowledge-what-this-console-does.md) — the knowledge
   base: what this console does, measured on this console.
5. [History](HISTORY.md) — why each rule exists.

Nothing later in that order overrides anything earlier.

## What is here

- [Manifest](manifest/README.md) — 12 parts, from the goal to the
  merged PS4 development standard
- [Rules](rules/README.md) — every rule, each on its own page
- [Amendments](amendments/README.md) — rules that later evidence changed
- [History](HISTORY.md) — the dated build log
- [Status board](STATUS.html) — generated: every check, the current
  state, what is still waiting on the console
- [RULES.index](RULES.index) — the same rules, machine-readable
- [Piglet notes](../PIGLET-NOTES.md) — what other PS4 homebrew already
  solved, and what is still ours to measure
- [Round notes](../ROUND-NOTES.md) — the round-by-round history (appendix)
- [Logging appendix](../APPENDIX-LOGS.md) — the log survey and dead ends
- [tools](../tools/) — the checkers that keep all of the above consistent

## Honesty note

Every claim here carries a status. `measured` means a log from this console
says so; `assumed` means nobody has checked yet. Numbers without a status are
a bug, and the checkers fail on them.

Source code lives in the project repository alongside this documentation;
packages are kept out of git history deliberately.
