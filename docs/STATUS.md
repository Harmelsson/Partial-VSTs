# Status

_Last updated: 2026-10-17 (session 3)_

## Sane v0.2.0 (step 1 of the rework) — waiting for the user's ears
Plan + diagnosis: `docs/SANE_V02_PLAN.md`. User decisions 2026-10-18: Seed-style drag-and-drop modulation; mockup 3 for the
exciter / EQ / modulation areas before building them; **modulation row goes on the bottom, above the keyboard**.
**Done in v0.2.0:** LOOP TIME (Keys / Hz / ms / BPM + key track; Delay core up to 2.1 s = echo-like), DIRECT (dry hit) knob,
output tapped after the loop processing (no raw noise unless DIRECT), every slot IN LOOP or AFTER LOOP, remove (×) + "off"
in the picker, per-type knob defaults, in-loop freq shift / chorus / ring mod always wet with C = spread (no thin tail),
loop level calibrated to ~1.0 (saturate / fold bite), velocity = loudness / position = timbre, hover help on every control,
middle-click = default, INIT button, bigger / darker text. Tests: all pass (`tools/SaneTests.cpp`, v0.2 section).
CPU (Linux): default 12 voices ~28 %; extreme (Partials, 5 slots incl. freq shift + chorus, 12 voices) ~83 % → optimise in v0.3.
**Mockup 3** (right board on the canvas, `docs/mockups/sane/V3.dc.html`): exciter envelope + lock/random + 3-band tone,
Dyn EQ slot + output EQ (tab next to HIT), modulation row above the keys (11 sources, editor, link list, white rings on knobs).
The per-note "velocity → X" bars are gone (replaced by modulation). **Next:** user verdict on mockup 3 → build v0.3.

### What to listen for (v0.2.0) — the "FX feel flat" checklist
1. **INIT** first. One Membrane hit = the plain loop. Now add one slot at a time and compare with INIT.
2. **Saturate IN LOOP**, drive 50 %+: the ring gets denser and brighter the harder you hit, and decays "compressed".
   Same slot **AFTER LOOP**: classic distortion on the output (the ring itself stays clean). Compare to any distortion plugin.
3. **Freq Shift IN LOOP**: shift ±, range small (2–10 Hz): every pass moves the overtones → a rising / falling "laser" swirl;
   SPREAD detunes each loop line differently. The tail should NOT get thin anymore at any setting.
4. **Disperse**: amount up, colour bright → overtones stretch apart (metallic, bell); colour dark → they squeeze together.
5. **Filter IN LOOP**: low-pass lower = the ring gets darker on every pass (felt / hand damping). Resonance up = a whistling mode.
6. **Loop TIME**: switch to Hz / ms: all keys ring the same loop (classic delay); KEY TRACK brings the keys back.
   Delay core + BPM + 1/8 + feedback ~60 %: a rhythmic echo drum that follows FL's tempo.
7. **DIRECT**: off = only the loop; up = the stick / noise attack on top.
8. Hover any control for its description; middle-click resets it.

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
