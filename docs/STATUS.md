# Status

_Last updated: 2026-09-26 (session 1)_

## Seed v0.5.0 — FX rack, BPM sync, release fix, Scala import/export/fit (PLAYGROUND.md "v0.5.0 changes")
Design, feature list and HOW TO USE: `docs/PLAYGROUND.md`. 86/86 headless checks pass
(`tools/SeedTests.cpp`; `--bench` CPU, `--release` release-shape probe). CPU (Linux): 16 voices ×
64 partials + FX ≈ 22 % of one core.
**Download: Release `build-14` (v0.5.0, green on Windows incl. all 86 tests):**
https://github.com/Harmelsson/Partial-VSTs/releases/tag/build-14

## What exists
- **Seed playground** (`plugins/Seed/`): calculator (Dissonance / Consonance / Imported .scl; hand
  edits of partials/notes, custom partials/notes, Scale ticks) strictly separate from the synth
  (key modes 12-EDO / Seed-12 / Scale / Magnet, life, glide, chords, pitch + freq shift, additive +
  subtractive, 5-slot FX rack, 4 modulators with Hz/BPM, Dice/Mutate/Reset per page, presets with
  All/Tuning/Sound scope). Every setting is a host parameter (automatable).
- **Shared kit** (`shared/`): `xen/` Stretch, Dissonance, Consonance, CalcModel, CalculatorWorker,
  Scala (.scl/.kbm + fit seed); `ui/` Theme, Controls, SquareLattice, CalculatorViews,
  EnvelopeView; `dsp/` SafetyStage, Modulator, PartialDelay, RoughReverb, ChirpReverb + Squash.
- Pipeline: GitHub Actions builds Windows VST3 + standalone on every non-docs push, runs tests,
  publishes a GitHub **Release** `Build N`.

## User should test (v0.5.0)
1. **Release**: does the tail still cut off? (Likely cause was re-pressing a key during its own
   release — fixed.) If yes: which key mode, release time, and was a key re-pressed?
2. **Curve key markers**: triangles = where your keys sit; Seed-12 shows 12-EDO dashes too.
3. **FX rack**: swap order (▲▼), Input Chain vs Partials, Picks morph (drag/wheel), Pan, Width.
   Chirp Reverb (Chirp knob), Squash Comp with Slope below 0 (over-compression).
4. **Sync**: set a modulator / the delay / Shimmer / Wander to BPM; change FL's tempo.
5. **Scala**: Import a .scl (e.g. from the Scala archive), play it, press *Fit seed*, listen to
   whether it gets smoother. Export .scl and load it into another synth.

## Next (user chooses)
- Partial-based FX from IDEAS.md (R1–R7), a distortion primer + tuned distortion.
- More synths using the shared calculator (plucks, drums, bells…) once Seed is curated.

## Open questions for the user
- Merge policy: each session runs on its own `claude/*` branch. Merge to `main` after each session,
  or let the agent merge? (Unmerged work may be invisible to the next session.)
