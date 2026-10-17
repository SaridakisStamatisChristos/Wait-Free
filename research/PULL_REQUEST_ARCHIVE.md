# Evidence-only pull request archive

The following pull requests were intentionally closed during productization because they are completed, superseded, diagnostic, or explicitly marked **do not merge**. Closing them removes ambiguity from the active review queue without deleting their branches, discussion, Actions history, or evidence.

| PR | Purpose | Disposition |
|---:|---|---|
| #19 | 2026-10-09 frozen evidence publication from an older source SHA | superseded by the current evidence architecture; preserved in PR history |
| #22 | hot-path v2 ablation laboratory | evidence-only; no production promotion |
| #25 | ARM64/GCC bulk-regression diagnostic | diagnostic; explicitly do not merge |
| #34 | isolated cursors + sequential ARM16 storage | evidence-only |
| #35 | wrapped physical cursor experiment | evidence-only |
| #37 | four-line wrapped control-state experiment | evidence-only |
| #42 | raw-pointer/Rigtorp code-generation experiment | evidence-only |
| #43 | dynamic consumer-shape experiment | completed; promotion rejected |
| #44 | owner-cache grouping experiment | completed; promotion rejected |
| #45 | consumer/layout factorial experiment | completed; promotion rejected |
| #46 | raw control-spacing experiment | completed; promotion rejected |
| #47 | full ARM raw-mechanism coverage | completed; promotion rejected |

No result is discarded by closing these PRs. Their exact commits and GitHub Actions runs remain the authoritative source for those experiments.
