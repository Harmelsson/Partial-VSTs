# Seed

Stretched-partial additive synth: the seed idea (from ArtHar's calculator) made playable in FL.
One number — the **seed** — bends the harmonic series so partial 2 lands on `seed:1` and every
partial n sits at `n^log2(seed)`. Seed 2 = ordinary harmonics (a soft saw).

## v0.1.0 (2026-09-26)
| Control | What it does |
|---|---|
| Seed (0.5–4) | The stretch. <1 = partials fall *below* the note (undertones), 1 = all partials collapse onto one pitch, 2 = normal, >2 = wide/bell-ish. |
| Partials (1–64) | How many partials. Changes fade in/out. |
| Rolloff (0–3) | Partial n loudness = 1/n^rolloff. 0 bright, 1 natural, 2+ dark. |
| Attack / Release | Envelope. |
| Gain | Output level (after it: the safety limiter, −1 dBFS ceiling). |
| Keys follow seed | Off: normal FL tuning. On: 12 keys span the seed's "octave" (middle C stays put). Glides when toggled. |
| Drone / Drone note | Holds a note without MIDI — scrub while listening. |

Everything is live: changes glide (~30 ms) on notes that are already sounding.
Mouse: drag up/right, Shift = fine, wheel = small steps, double-click = type, middle/Alt-click = default.

## Code map
- `Source/SeedEngine.h` — the DSP (JUCE-free): voices, rotator oscillators, control-rate smoothing.
- `Source/PluginProcessor.*` — parameters, MIDI, drone, safety stage, state.
- `Source/PluginEditor.*` — UI + animated partial view.
- Tests: `tools/SeedTests.cpp`.

## Next (see docs/SEED_NOTE_LIFE.md)
v0.2: the keyboard plays the seed's own *scale* (dissonance-curve dips), and held notes survive
the scale changing under them — the note birth/death design.
