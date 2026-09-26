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

## 2026-09-26 — session 1 — ArtHar calculator discovered & reviewed
**Context:** User revealed the ArtHar calculator already exists (Rust, v1.0, "advanced and usable",
no sound) and should be the base. It lives in private repo `Harmelsson/arthar`, which another
cloud session is modifying right now.

**Done:**
- Attached `Harmelsson/arthar` read-only, cloned snapshot `8bf556f`, read ARCHITECTURE, GLOSSARY,
  PROJECT_LOG and the calculator's stretch/loudness/dissonance/scale code.
- Wrote `docs/ARTHAR_REFERENCE.md` (summary + how to attach + integration decision).
- Updated CLAUDE.md and IDEAS (Seed = the calculator made playable).

**Decision (reversible, flagged to user):** port needed math to C++ in `shared/xen/`, use the Rust
crate as a test oracle — not FFI-linking. Reasons in ARTHAR_REFERENCE.md.

**Failed / blocked:** `cargo test` on arthar here fails — container has rustc 1.94.1, arthar needs
1.95. Fix when needed: `rustup update`.

**Next:** user answers: first plugin, merge policy, artifact download, and OK on port-to-C++.
