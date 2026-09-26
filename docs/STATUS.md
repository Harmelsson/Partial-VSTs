# Status

_Last updated: 2026-09-26 (session 1)_

## Seed Playground v0.3.1 — built while the user was away
Design, feature list and HOW TO USE: `docs/PLAYGROUND.md`. User's job on return: try it, decide
what to keep / delete / split into a new synth / change. 59/59 headless checks pass.
`SeedTests --bench` measures CPU.

## What exists
- **Seed v0.3.0 playground** (`plugins/Seed/`): calculator (dissonance + consonance, hand edits
  of partials/notes, custom partials/notes) strictly separate from the synth (key modes 12-EDO /
  Seed-12 / Scale / Magnet, life, glide, pitch + freq shift, additive + subtractive tools, Partial
  Delay, Rough Reverb, 4 modulators, Dice/Mutate, presets with All/Tuning/Sound scope).
  ~300 automatable params. 55 headless checks (`tools/SeedTests.cpp`).
- **Shared kit for all future synths** (`shared/`): `xen/` Stretch, Dissonance (copy of ArtHar's
  method), CalculatorWorker (background thread); `ui/` Theme (ArtHar two-colour), Controls
  (per-digit ScrubBox, SegmentedToggle), SquareLattice (square-dot plots + afterglow),
  CalculatorViews (curve / partials / scale table), EnvelopeView; `dsp/` SafetyStage.
- Pipeline: GitHub Actions builds Windows VST3 + standalone on every non-docs push, runs tests,
  publishes a GitHub **Release** `Build N` (front page → Releases). Run #2 = first green build;
  Release `build-5` = v0.1.0; **Release `build-6` = Seed v0.2.0** (green, 08:00 UTC):
  https://github.com/Harmelsson/Partial-VSTs/releases/tag/build-6

## User should test (Seed v0.3.0) — see docs/PLAYGROUND.md "How to use it"
Older v0.2 checklist below still applies.
## (v0.2.0 checklist)
1. Look & feel vs ArtHar. Scrub digits of Seed while a note is held: curve, table, partials and
   the sound should all move together.
2. Velocity: soft vs hard notes. Envelope: drag the three hollow corners; try Sustain 0 (plucks).
3. Fundamental changes the curve (roughness depends on Hz); with Keys = Seed it also moves key 60.
4. Min dip strength: more/fewer notes in the table. Partials 64: curve still responsive?
5. Scrub sensitivity: 18 px per step — too slow/fast?

## Next
- **Seed v0.2**: user wants to TRY SEVERAL of the ideas in `docs/SEED_NOTE_LIFE.md` ("variety,
  no harm"). Plan: a "note life" mode selector, starting with Magnet keyboard + Life knob, then
  Valley riders / Mitosis / Orphans / Degree keyboard as extra modes. Needs C++ copy of the
  dissonance curve first.
- Possibly: per-digit scrubbing for the seed value (ArtHar-style) if the knob is too coarse.

## Open questions for the user
- Merge policy: each session runs on its own `claude/*` branch. Merge to `main` after each session,
  or let the agent merge? (Unmerged work may be invisible to the next session.)
