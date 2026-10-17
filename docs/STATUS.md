# Status

_Last updated: 2026-10-17 (session 3)_

## NEW: drum plugin (feedback synthesis) — RESEARCH DONE, waiting for user decisions
Full research + options: `docs/DRUM_RESEARCH.md` (drum physics, feedback structures, partial-FX-in-loop
methods, 4 architectures A/B/C/D, exciters, Seed tuning ecosystem T1–T4, UI direction). Nothing built yet.
**Next:** user answers "Decisions for the user" (end of that doc) → agent makes a visual mockup in the
desert palette (desaturated desert yellow + sharp black/white) → user approves → build.

## Seed v0.7.0 — feedback batch 2 (drawn LFOs, modulate everything, locks, window scale)
Design + HOW TO USE: `docs/PLAYGROUND.md` ("v0.7.0 changes"). 163 headless checks pass
(`tools/SeedTests.cpp`; `--bench` CPU, `--release` release-shape probe).
**Download: the newest Release on the repo's front page (built by CI from the pushed commit).**
Not yet tested in FL by the user — see "User should test". Two things could NOT be tested here and are
the first to check: the keyboard-focus fix (Windows only) and the window scale inside FL.

## What exists
- **Seed playground** (`plugins/Seed/`): calculator (Dissonance / Consonance / Imported .scl /
  **EDO / Seed-EDO** 1–120 steps; hand edits; **scale lock + partials lock**) strictly separate from the
  synth (the keyboard always plays the scale from the **base key**; life, glide, chords, pitch + freq shift,
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

## User should test (v0.7.0)
1. **Keyboard focus** (Windows): click inside the plugin, then play FL's typing keyboard — does it sound without
   clicking the window border? (After a click the plugin hands the focus back to FL's window.)
2. **Window scale** (header, under the version): − / 100 % / + , 25–200 %. Does FL's window follow?
3. **Modulate everything**: drag a number chip (right column) onto a switch, a choice (Scale method!), an
   envelope corner, an EQ node, a scale note on the curve, a partial in the plot, a tick box, a rack row.
4. **Click a link chip** (e.g. "FX 4 squash makeup"): tab opens, control glows orange. Drag the chip = amount.
5. **LFO editor**: click a modulator row; Presets, drag squares/dots, double-click = point, right-click = delete,
   Shift+drag = grid snap (X / Y grid buttons). Trigger Never / MIDI / Audio (source Input / Output / Sidechain +
   threshold), Loop / One-shot. (Input / Sidechain only carry audio if FL connects them — tell me what FL does.)
6. **Lock partials** (calculator row): lock, move the seed — timbre stays, notes still follow the seed. Unlock asks.
7. **No more Key mode / Magnet**: Scale method EDO 12 = normal piano, Seed-EDO 12 = old Seed-12. The curve always shows
   the real notes. Fundamental + Base key decide where the first note is.

## Next (user chooses)
- **Drum plugin decisions** (see top) — the user's current priority.
- FX feedback (user announced it comes after this batch).
- Partial-based FX from IDEAS.md (R1–R7), a distortion primer + tuned distortion.
- More synths using the shared calculator once Seed is curated.

## Open questions for the user
- Magnet (keys pulled to the nearest note) went with the Key modes. Missed, or fine?
- Lock buttons don't take modulators (they carry a snapshot). Other controls do.
- Merge policy decided 2026-09-30: agent keeps `main` up to date after every task.
