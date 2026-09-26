# Partial-VSTs — Agent Instructions (read first, every session)

Experimental **JUCE audio plugins (VST3) for FL Studio on Windows**, exploring xenharmonic /
partial-based ideas — and generally *weird*, unconventional sound tools.

- **User = tester + vision director.** Cannot code. Tests by ear in FL Studio, reports symptoms,
  steers direction. **Agent = the only coder.**
- **Weird is the default.** The user explicitly does not want yet-another-conventional plugin
  (there are 1000s). Favor strange, exploratory, "what happens if…" behavior. Simple tools are fine
  when asked for or when they serve as building blocks.
- **ArtHar is a completely separate project** (standalone Rust app, `Harmelsson/arthar`). It will
  never host VSTs; it runs its own in-house plugins. The two share *idea origins* and often
  *mechanism logic* — never code, never integration. If the user wants something to exist in both,
  they will ask for a full rewrite/rethink for that system, not a bridge.
  **Only read the arthar repo when the user explicitly points you at something there** (e.g. "port
  plugin X from ArtHar"). Math already copied from it is summarized in `docs/ARTHAR_REFERENCE.md`.
  Never push to arthar. `xenharmonic_software_notes.md` / `questions.md` in this repo are old ArtHar
  planning docs (math reference only). `Concepts/` = ArtHar art; ignore.
- **Everything is dynamic (user's core concept rule).** Any parameter change must be heard live,
  including on notes that are already held — e.g. changing the seed while holding a note glides
  that note's partials (and pitch, if the keyboard follows the seed) in real time. No "change takes
  effect on next note". Design every plugin around smooth, click-free live changes.

---

## ⚠️ Session protocol — sessions can end at ANY moment, without warning

The user will not say "we're done". Any response might be the last one. Therefore:

1. **On session start:** read `docs/STATUS.md` (current state + next step), then the last few
   entries of `docs/LOG.md`, then `docs/LESSONS.md` if touching build/DSP/FL-specific stuff.
   Run `git fetch --all` and check whether other `claude/*` branches hold newer work than yours
   (sessions get different branches; unmerged work may live elsewhere). Merge it in if so.
2. **After EVERY completed task (not every session):**
   - Update `docs/STATUS.md` (what exists now, what's next, what the user should test).
   - Append an entry to `docs/LOG.md` (what was done, what worked, what failed, why).
   - Add to `docs/LESSONS.md` if something failed or surprised us (so it's never repeated).
   - Update `docs/IDEAS.md` if ideas were added/tried/killed.
   - **Commit and push immediately.** Never end a response with uncommitted work.
3. **Long tasks:** checkpoint mid-way. Commit partial progress with a `STATUS.md` note marked
   `IN PROGRESS` describing exactly what's half-done and what the next concrete step is.
4. **Failures are data.** Log dead ends honestly (what was tried, symptom, suspected cause).
   Don't delete failed experiments from the log; mark them.
5. **User feedback** (by-ear reports) goes verbatim-ish into `LOG.md` under the relevant plugin,
   so later sessions know *why* things changed.

## Project rules

- **Hearing safety is non-negotiable.** Every plugin's output passes through the shared safety
  stage (NaN/Inf scrub → DC block → hard ceiling below 0 dBFS). Weird DSP (feedback, resonators,
  random spectra) blows up easily; the user listens on headphones/laptop audio. Never bypassable.
- **Every plugin shows its version + build id in its UI**, and the version is bumped on every
  change, so the user can confirm FL Studio loaded the new build.
- **Verify before handing over.** The user can't debug code. Before a build goes to the user:
  it must compile, and DSP should be checked headlessly where possible (render to WAV / numeric
  checks in `tools/`) — no NaNs, no runaway levels, math matches expectation (e.g. seed 2.0 ⇒
  plain harmonic series).
- **Small, testable-by-ear increments.** Each handover should have a short "what to listen for"
  list in `STATUS.md`.
- Comment code for a future agent with zero context: explain *why*, not just *what*.
- Keep docs short and current; `STATUS.md` must stay skimmable (<~100 lines).

## Planned repo layout (not all built yet — check `STATUS.md` for what exists)

```
CMakeLists.txt            top-level; pulls JUCE (pinned version) via FetchContent
shared/                   code reused by all plugins (xen math, safety stage, UI bits)
plugins/<Name>/           one folder per plugin (Processor/Editor/README.md)
tools/                    headless test renderers / math checks (run on Linux in-session)
.github/workflows/        CI: builds Windows VST3s → downloadable artifact for the user
docs/                     STATUS, LOG, LESSONS, IDEAS, TESTING (user install/test guide)
```

Build targets: VST3 (FL Studio, Windows) is what matters. Standalone target is optional.
The agent's container is Linux: compile + headless tests happen here; the Windows `.vst3`
comes from GitHub Actions.

## Doc map

| File | Purpose | Update when |
|---|---|---|
| `docs/STATUS.md` | Current state, next step, what to test, open questions | every task |
| `docs/LOG.md` | Append-only chronological log incl. failures + user feedback | every task |
| `docs/LESSONS.md` | Gotchas (JUCE, FL Studio, CI, DSP) — "don't repeat this" | on failure/surprise |
| `docs/IDEAS.md` | Plugin idea backlog + status (idea/tried/shipped/killed) | when ideas change |
| `docs/ARTHAR_REFERENCE.md` | ArtHar calculator summary + integration decision | when porting/re-syncing |
