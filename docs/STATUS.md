# Status

_Last updated: 2026-09-30 (session 2)_

## Seed v0.6.0 — user feedback batch 1 (modulator column, EQ, scale lock, EDO…)
Design + HOW TO USE: `docs/PLAYGROUND.md` ("v0.6.0 changes"). 126 headless checks pass
(`tools/SeedTests.cpp`; `--bench` CPU, `--release` release-shape probe). CPU (Linux): 16 voices ×
64 partials + EQ + sends + FX ≈ 18 % of one core.
**Download: the newest Release on the repo's front page (built by CI from the pushed commit).**
Not yet tested in FL by the user — see "User should test".

## What exists
- **Seed playground** (`plugins/Seed/`): calculator (Dissonance / Consonance / Imported .scl /
  **EDO / Seed-EDO** 1–120 steps; hand edits; **scale lock**) strictly separate from the synth (key
  modes 12-EDO / Seed-12 / Scale / Magnet, **base key**, life, glide, chords, pitch + freq shift,
  additive tools, **5-band key-tracked EQ**, **12-slot FX rack**, presets All/Tuning/Sound). Every
  setting is a host parameter (≈ 800). Tabs: Keys & pitch · Envelope · Timbre · FX.
- **Modulator column** (right side): 8 modulators; drag a numbered chip onto ANY float number box
  to link it (one modulator → many fields, each link with its own amount). Hz-or-BPM in one
  control; divisions come as straight / triplet / dotted (4/16, 4/16t, 4/16.).
- **Shared kit** (`shared/`): `xen/` Stretch, Dissonance, Consonance, CalcModel, CalculatorWorker,
  Scala; `ui/` Theme, Controls (ScrubBox, RateControl…), SquareLattice, CalculatorViews,
  EnvelopeView; `dsp/` SafetyStage, Modulator, **Eq**, PartialDelay, RoughReverb, ChirpReverb.
- Pipeline: GitHub Actions builds Windows VST3 + standalone on every non-docs push, runs tests,
  publishes a GitHub **Release** `Build N`.

## User should test (v0.6.0)
1. **Release**: does the tail still cut off? (Was a hard cut at −60 dB; now fades to −100 dB first.)
2. **Note lighting** on the curve: short notes, and in 12-EDO / Seed-12 / Magnet (nearest note lights).
3. **Lock scale** (Calculator row): lock, change the seed, notes stay; click LOCKED → warning popup.
4. **Base key** (Keys & pitch): set it to the key you start on; that key = the scale's first note.
5. **Scale method = EDO / Seed-EDO**, scroll *Steps* 1–120; notes show on curve + Scale table.
6. **Modulators**: drag chip "1" onto Seed / an FX Mix / an EQ Freq box; right-click a modulated box or
   a link chip to remove; drag a link chip up/down for its amount; try BPM + scrolling divisions.
7. **FX tab**: 12 slots (scroll the list), EQ on the right (drag nodes, wheel = Q, Key track).
8. Hover anything unclear (Life, Bend range, Formant, Shimmer, Chaos…) for an explanation.

## Next (user chooses)
- FX feedback (user announced it comes after this batch).
- Partial-based FX from IDEAS.md (R1–R7), a distortion primer + tuned distortion.
- More synths using the shared calculator once Seed is curated.

## Open questions for the user
- Window is now 1592 × 960 (mod column). Too big for the screen? (Could add a resizable/scaled UI.)
- "12-EDO and Seed-12 on the curve/scale window, number 1–120": built as two new Scale methods (EDO,
  Seed-EDO). The Key modes "12-EDO" / "Seed-12" themselves still mean normal tuning / 12 keys per
  octave. Is that what was meant?
- Merge policy decided 2026-09-30: agent keeps `main` up to date after every task.
