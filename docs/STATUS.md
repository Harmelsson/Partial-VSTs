# Status

_Last updated: 2026-10-17 (session 3)_

## Sane v0.1.0 — first playable build (feedback drums) — waiting for the user's ears
Design: `docs/DRUM_RESEARCH.md`, mockup `docs/mockups/sane/` (https://claude.ai/artifact/MbhbEy59oop9ECwuRGzZau).
Code: `plugins/Sane/` (README there). Tests: `tools/SaneTests.cpp` — all pass (pitch within 0.1 c in every core,
safety at every extreme incl. self-oscillation, click-free morph, levels, tension/pitch drop/length, state).
CPU (Linux): default patch with 12 ringing voices ~18 % of a core; heaviest (Partials, 5 inserts) ~59 %.
**Download:** the newest Release (contains Seed.vst3 AND Sane.vst3). First time: FL → Manage plugins → Find installed.

### User should test (listen for)
1. **Basics:** play low keys (C1–C3). Membrane core = tom-ish. Window opens at 90 % (− / + top right).
2. **Every note is different:** repeat one key — the noise changes each time (Exciter shows the last 3 in black).
   "random per note" = how much.
3. **Strike:** soft notes ring "edgier", hard notes "thumpier" (velocity → position). Click the round head to move the base.
4. **Cores:** Membrane / Partials (pure, bell/tabla-like) / Delay (plucky, string-like until Disperse or Flip).
5. **Loop slots:** click a slot, change its type (TYPE ▾) and knobs WHILE a note rings; drag a slot onto another to swap.
   Try Flip (50 % = noise drum), Freq Shift (spirals), Fold, Ring Mod. FEEDBACK at the very top = self-oscillation
   (holds while the key is held, stops on release).
6. **Modes:** A = Physical (pick Membrane/Timpani/Tabla/Bar/Plate), B = Calc (seed); slide MORPH while ringing.
7. **Hit:** drag the black squares (pitch drop amount / time) and the white one (length). TENSION = hard hits start sharp.
8. **Keys strip:** click keys (low on a key = loud); they light up and fade.
Not in v0.1 (shown dashed): kit drawer, link groups, scale keys (12-EDO for now), audio-in exciter, picked-partial targeting.

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
- **Sane v0.2** after the user's first listen: kit drawer, link groups (★ leader wins), Seed scale keys, CPU work.
- FX feedback (user announced it comes after this batch).
- Partial-based FX from IDEAS.md (R1–R7), a distortion primer + tuned distortion.
- More synths using the shared calculator once Seed is curated.

## Open questions for the user
- Magnet (keys pulled to the nearest note) went with the Key modes. Missed, or fine?
- Lock buttons don't take modulators (they carry a snapshot). Other controls do.
- Merge policy decided 2026-09-30: agent keeps `main` up to date after every task.
