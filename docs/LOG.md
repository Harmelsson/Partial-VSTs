# Log (append-only, newest at bottom)

Entry format:
`## YYYY-MM-DD — session N — short title` then Done / Worked / Failed / User feedback / Next.

---

## 2026-09-26 — session 1 — project kickoff & workflow
**Context:** Repo had ArtHar notes (`xenharmonic_software_notes.md`, `questions.md`) and ArtHar
concept art. User: ArtHar moves to a separate standalone project; this repo is for experimental
JUCE VSTs for FL Studio. User = tester/vision director, agent = coder. Wants weird, unconventional
plugins. Sessions may end without warning → document after every task.

**Done:**
- Created `CLAUDE.md` (session protocol, project rules, planned layout).
- Created `docs/STATUS.md`, `docs/LOG.md`, `docs/LESSONS.md`, `docs/IDEAS.md` (13 seed ideas).
- Checked container: cmake/g++/clang/ninja present, JUCE repo reachable → local compile + headless
  DSP tests are possible; Windows VST3 must come from GitHub Actions.

**Decisions:**
- JUCE + CMake, VST3 target, Windows via CI.
- Shared non-bypassable safety stage in every plugin (carried over from ArtHar's hearing-safety rule).
- ArtHar docs kept as math reference only; their tech/design decisions don't apply.

**Next:** user picks first plugin (Seed vs Stretch Resonator) → Task 0 pipeline + that plugin.
