# Status

_Last updated: 2026-09-26 (session 1)_

## What exists
- **Seed v0.1.0** (`plugins/Seed/`, see its README): stretched-partial additive synth, 64 partials,
  16 voices, live glides on held notes, keys-follow-seed, drone, gentle custom UI.
  22 headless checks pass on Linux (`tools/SeedTests.cpp`).
- Pipeline: CMake + JUCE 8.0.15 (FetchContent). GitHub Actions (`.github/workflows/build.yml`)
  builds Windows VST3 + standalone, runs tests, uploads zip `PartialVSTs-windows-build-N`.
  CI run #1 (commit 026c59f) was in progress at time of writing — check result first.
- `shared/`: `xen/Stretch.h` (copied math), `dsp/SafetyStage.h` (non-bypassable), `ui/` (theme,
  SmoothKnob, SmoothToggle).
- Docs: CLAUDE.md, docs/{STATUS, LOG, LESSONS, IDEAS, TESTING, ARTHAR_REFERENCE, SEED_NOTE_LIFE}.

## User should test (Seed v0.1.0) — install guide: docs/TESTING.md
1. Seed 2.0, hold a note: should sound like a soft, plain saw-ish tone.
2. Hold a note/chord and scrub Seed slowly: partials should glide, never click.
3. Seed below 1 (undertones), exactly 1 (collapse), 3–4 (bells): weird on purpose.
4. Partials / Rolloff while holding: smooth fades.
5. "Keys follow seed" on: 12 keys = the seed's octave; toggling while holding glides.
6. Drone on: sound without MIDI. Middle-click resets a knob, double-click types a value.
7. Knob feel: too slow/fast? Bottom-right label must show the downloaded build number.

## Next
- **Seed v0.2**: user picks from `docs/SEED_NOTE_LIFE.md` (agent recommends Magnet keyboard +
  Life knob). Needs C++ copy of the dissonance curve.
- Possibly: per-digit scrubbing for the seed value (ArtHar-style) if the knob is too coarse.

## Open questions for the user
- Birth/death design pick (SEED_NOTE_LIFE.md).
- Merge policy: each session runs on its own `claude/*` branch. Merge to `main` after each session,
  or let the agent merge? (Unmerged work may be invisible to the next session.)
