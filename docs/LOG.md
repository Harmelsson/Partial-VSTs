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

## 2026-09-26 — session 1 — user can't find the build → Releases
**User:** looked at the repo front page (main branch), saw no "green" anything, didn't know where
builds live. Also: on note birth/death — "we will test out different ideas, there is no harm in
creating variety" → implement several as selectable modes.
**Done:** CI publishes every successful build as a GitHub Release (front page → Releases box);
TESTING.md rewritten accordingly. Windows CI run #1: configure OK (4.5 min), build in progress.

## 2026-09-26 — session 1 — first green Windows build
- CI run #2 succeeded (compile + SeedTests on Windows). Sent the user the run link (artifact).
- Tried to download the artifact to send it directly: blocked by the container's proxy.
- User asked whether the agent could build Windows itself faster → no (Linux container,
  cross-compile untestable). Instead: docs-only pushes skip CI, superseded runs are cancelled.

## 2026-09-26 — session 1 — user tested v0.1 → v0.2 requests
**User feedback (build #2):** "holy moly, really nice for a first build!!!" but "looks kinda shit" →
redesign in ArtHar's dark style (sent an ArtHar v1.0 screenshot: near-black, off-white, thin frames,
letter-spaced caps section titles, number boxes with arrows, square-dot plots, scale table right).
Requests: velocity-dependent note volume; decay + sustain with an envelope in ArtHar style; the
dissonance-curve calculator on top. Vision: every synth gets the same/similar calculator on top;
calculator decides where notes/partials go; per synth some rules stay default, some modifiable,
some heavily modified. Synths will serve different purposes (plucks, drums, bells, whack sound
design). Seed = exploration for now.
**Plan v0.2.0:** C++ copy of dissonance.rs (curve, dips, strength, golden-section refine) in a
background worker; ArtHar-style shared UI kit (two-colour theme, per-digit ScrubBox, segmented
toggles, square lattice plots with easing + afterglow, scale table); ADSR + velocity sensitivity;
Fundamental + Min dip strength params. Keyboard does NOT play the found scale yet (that's the
note-life work, next).

## 2026-09-26 — session 1 — Seed v0.2.0 (ArtHar redesign + calculator + ADSR)
**Done:** C++ copy of ArtHar dissonance.rs (`shared/xen/Dissonance.h`, differences documented in
its header: pruned negligible pairs, grid ≤2000 points, |period| for seeds < 1) + background
worker; ArtHar-style UI kit in `shared/ui/`; Seed editor rebuilt (1160×800); new params
fundamental, minDip, decay, sustain, velocity (version hint 2); engine ADSR + velocity curve
(vel^(2·sens)) + fundamental as key-60 anchor in Seed-keys mode.
**Verified:** 35/35 checks. Curve copy matches ArtHar's pinned seed-2 notes to 0.1 c and seed 2.5's
strongest dip (927.93 c). 64 partials: ~90 ms per curve.
**Failures along the way (fixed):** `Font::getStringWidthFloat` gone in JUCE 8; a member named
`hitTest` hid Component::hitTest; sustain 0 never ended notes (smoothed value never reached 0 →
smoothing now snaps); "·" rendered as "Â·" (JUCE reads const char* as Latin-1); scale table clipped.
**Found, not a bug:** seed 0.5 (undertones) has only shallow dips (fifth ≈1.5%) → with the default
2% cut-off its scale is empty. Musically real: loud partials in the bass = smooth roughness.
