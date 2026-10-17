# Sane — feedback-synthesis drums ("Insane in the membrane")

No oscillators: a noise burst hits a bank of tuned delay LOOPS, and effects run INSIDE the loops, so every
pass changes the sound again. Why and how: `docs/DRUM_RESEARCH.md`. Look: `docs/mockups/sane/` (desert theme).

| File | What |
|---|---|
| `SaneParams.*` | every parameter (IDs never change), slot types + what their knobs mean |
| `SaneDsp.h` | mode tables (membrane Bessel, timpani, tabla, bar, plate), Calc/File sources, the noise burst, the 9 insert effects |
| `SaneEngine.h` | voices: Delay (1 loop) / Membrane (8-line lossless FDN) / Partials (16 band-passed loops), pitch drop + tension, governor |
| `PluginProcessor.*` | params -> engine, MIDI, safety stage, state |
| `SaneLook.h`, `SaneControls.h`, `SaneViews.h`, `PluginEditor.*` | the UI (aligned grid, every value also drawn) |

Tests: `tools/SaneTests.cpp` (`--bench`, `--wav dir`, `--snapshot x.png`).

Loop per sample: delay (Lagrange) -> coupling (Givens rotations) -> 7 slots -> (band-pass) -> damping LP -> DC guard ->
gain -> guard (soft clip at 2.5) -> + exciter. LP and DC-guard phase/gain at the mode are compensated, so modes sit
within 0.1 cent of their target. A per-voice output high-pass (outside the loops) removes sub-bass.
