# Seed

> Current version and feature list: `docs/PLAYGROUND.md` and `docs/STATUS.md` (this README is the v0.1-0.2 history).

Stretched-partial additive synth: the seed idea (from ArtHar's calculator) made playable in FL.
One number — the **seed** — bends the harmonic series so partial 2 lands on `seed:1` and every
partial n sits at `n^log2(seed)`. Seed 2 = ordinary harmonics (a soft saw).

## v0.2.0 (2026-09-26) — ArtHar look, calculator on top, full envelope
**Calculator** (top bar, same inputs as ArtHar): Fundamental (Hz) · Seed · Partials · Rolloff ·
Min dip strength (%). The dissonance curve + scale are recalculated live on a background thread.
The keyboard does NOT play the found scale yet (that's the note-life work, next).
- *Fundamental*: the pitch the curve is calculated at (roughness depends on real Hz), and key 60's
  pitch when Keys = Seed.

| Control | What it does |
|---|---|
| Seed (0.5–4) | The stretch. <1 = undertones, 1 = collapse, 2 = normal, >2 = wide/bell-ish. |
| Partials (1–64) / Rolloff (0–3) | How many partials / how fast they get quieter (1/n^rolloff). |
| Envelope A/D/S/R | Drag the hollow corners in the envelope picture, or scrub the boxes. Decay glides towards sustain; everything is live on held notes. Sustain 0 = plucks (notes end by themselves). |
| Velocity (%) | 100% = volume follows velocity fully (vel², velocity 64 ≈ −12 dB); 0% = all notes equal. |
| Gain (dB) | Output level (then the safety limiter, −1 dBFS ceiling). |
| Keys: 12-EDO / Seed | Normal FL tuning, or 12 keys span the seed's "octave" with key 60 = Fundamental. Glides when switched. |
| Drone Off/On + note | Holds a note without MIDI. |

Numbers: drag a DIGIT up/down (18 px per step, Shift = 3× slower), wheel over a digit, arrows,
double-click to type, middle-click (or Alt+click) = default.

## v0.1.0 (2026-09-26)
First build: additive synth, knobs, drone, keys-follow-seed. User: "really nice for a first build",
UI "looks kinda shit" → v0.2 redesign.

## Code map
- `Source/SeedEngine.h` — the DSP (JUCE-free): voices, rotator oscillators, control-rate smoothing.
- `Source/PluginProcessor.*` — parameters, MIDI, drone, safety stage, state.
- `Source/PluginEditor.*` — window layout (uses the shared ArtHar-style kit in `shared/ui/`).
- Shared: `shared/xen/Dissonance.h` (curve + dips), `shared/xen/CalculatorWorker.h` (background
  thread), `shared/ui/{Theme,Controls,SquareLattice,CalculatorViews,EnvelopeView}.h`.
- Tests: `tools/SeedTests.cpp`.

## Next (see docs/SEED_NOTE_LIFE.md)
v0.3: the keyboard plays the seed's own *scale* (dissonance-curve dips), and held notes survive
the scale changing under them — the note birth/death design.
