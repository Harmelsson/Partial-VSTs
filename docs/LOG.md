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

## 2026-09-26 — session 1 — relationship clarified, Seed chosen
**User:** ArtHar and the VSTs are *completely different things*: ArtHar will never run VSTs; the
link is shared idea-origins and overlapping mechanism logic, nothing else. If something should
exist in both, the user asks for a total rewrite for that system. Arthar files are for reference
only, read only when the user points at a plugin there. "Copy what you can, that's the first step."
**User on Seed:** loves it. Core concept rule: *everything is dynamic* — changing the seed while
holding a note must be heard live; since notes' positions change, notes may be created/terminated
while playing → wants ideas for that problem. Seed is a synth → smoother, less intimidating UI,
gentler scrubbing/scrolling.
**Done:** CLAUDE.md + ARTHAR_REFERENCE.md rewritten to match (no oracle/linking, dynamic rule).

## 2026-09-26 — session 1 — Seed v0.1.0 + build pipeline
**Done:**
- CMake/JUCE 8.0.15 scaffold, shared safety stage + stretch math + gentle UI widgets.
- Seed v0.1.0: additive synth with rotator oscillators, control-rate (32 samples) smoothing
  (~30 ms), power-normalised voices, Nyquist and sub-audio fades, 16-voice stealing, drone,
  pitch bend ±2 st, keys-follow-seed (blends 100 c/key ↔ period/12 per key, so toggling glides).
- `tools/SeedTests`: 22 checks — all PASS on Linux (spectrum 1/n exact to 3 decimals, seed-2.5
  partial 2 at 1100 Hz, glide max-step 0.099 vs steady 0.078, worst-case peak = 0.794, state).
- UI snapshot checked via xvfb (`SeedTests --snapshot`); first layout had overlaps → fixed.
- GitHub Actions Windows workflow + docs/TESTING.md (install guide for the user).
- docs/SEED_NOTE_LIFE.md: 6 ideas for notes born/dying while held (user asked for ideas).
**Worked first try:** compile (only -Wfloat-equal warnings, fixed with juce::exactlyEqual).
**Not verified yet:** the Windows CI build itself, and anything in real FL Studio.
