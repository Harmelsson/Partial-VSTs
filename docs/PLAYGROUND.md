# Seed Playground (v0.3.x) — design + progress

_User brief (2026-09-26, then away for a few hours): "Build this synth into an experimental
playground… a drastic chaotic mess of changes, then I decide what I keep, delete, split into a new
synth, or change." ~1.5 h of experimentation._

## Must-haves from the brief → where they live
| Brief | Implementation |
|---|---|
| Pitch shifting | `pitch` (cents ±2400), `fshift` (Hz ±1000, adds Hz to every partial → inharmonic), bend range |
| Everything automatable | every editable value is a host parameter (≈300), incl. per-partial and per-note edits |
| Presets; reuse a tuning with a new sound | preset bar; load scope **All / Tuning / Sound** (Tuning = calculator group) |
| Disable / add custom notes (ArtHar) | per-degree `n{d}_on`, `n{d}_off` (cents offset); 8 custom note slots `cn{k}_*` |
| Custom partials + loudness (ArtHar) | per-partial `p{n}_lvl` (dB offset), `p{n}_pos` (cents offset); 8 custom partial slots `cp{k}_*` |
| Consonance method (ArtHar) | `method` = Dissonance / Consonance; `tolerance`, `minShared` (C++ copy of consonance.rs) |
| Additive / subtractive tools | tilt, odd/even, formant, shimmer, stereo spread; noise; per-voice SVF filter + filter env |
| Random modifiers | 4 mod slots (sine/tri/saw/square/S&H/smooth random/drift → any target) + Dice / Mutate |
| Modified calculator data marked; removing returns to default | edits are offsets with a neutral value (0 dB / 0 c / on); modified items drawn **orange** + hollow square; middle-click removes the edit |
| Synth separate from calculator | Calculator = tuning group of params + worker in the processor; Synth only *reads* its partials + scale |
| Creative FX | **Partial Delay**: only chosen partials are sent (rules: odd, even, high, low, every 3rd…), "spiral" pitch drift per repeat. **Rough Reverb**: comb resonators tuned to the scale's notes, each with a detuned twin; "Roughness" = detune as a fraction of the Plomp-Levelt worst-clash distance (100% = maximally uncomfortable) |

## Keyboard modes (how keys become pitches)
- **12-EDO**: normal FL tuning.
- **Seed-12**: 12 keys span the seed's "octave", key 60 = Fundamental (old "Keys follow seed").
- **Scale**: key 60 = unison; each key = next *playable* note of the calculator's scale, wrapping
  every period. When the scale changes while held, voices glide to their key's new meaning
  (note-life idea 6). **Life** fades notes by their strength (idea 2).
- **Magnet**: Seed-12 keys pulled toward the nearest playable note by **Magnet** % (idea 5).

## Progress (update as phases land)
- [x] A core: consonance copy, edited partials/notes model, processor-owned calculator worker,
      key modes, glide, life, pitch/freq shift
- [x] B plot editing: drag/add/remove partials + notes, scale table toggles, orange markers
- [x] C timbre: additive tools, stereo spread, noise, filter
- [x] D FX: partial delay, rough reverb
- [x] E modulators + dice/mutate
- [x] F presets
- [x] G tabs UI, snapshot, docs  → **v0.3.0 done: 55/55 headless checks**
- [x] v0.3.1 chaos toys: chord, wander, scramble (59/59 checks). CPU (Linux): 1 voice ≈1%,
      16 voices × 64 partials + filter + sends + FX ≈17% of one core.

## How to use it (for the user)
- **Calculator (top)**: Fundamental, Seed, Partials, Rolloff, Scale method (Dissonance / Consonance)
  + Min dip (dissonance) / Tolerance + Min shared (consonance). *Reset edits* clears every hand edit.
  *Dice tuning* randomises the calculator.
- **Curve**: drag a note sideways = move it · double-click empty space = add a custom note (8 slots)
  · right-click = switch a note off/on · middle-click = back home. Orange = modified.
- **Partials**: drag up/down = loudness, sideways = move (axis locks after a few px) · double-click
  = add a custom partial (8 slots) · right-click = mute/unmute · middle-click = remove the edit.
- **Scale table**: click the square = on/off; middle-click = reset.
- **Presets (header)**: Save as… (type name, Enter). `<` `>` step, click the name for the list.
  *Load: All / Tuning / Sound* decides what a preset load replaces → "same tuning, other sound".
  Files: Documents/Partial-VSTs/Seed Presets/*.seedpreset.
- **Synth tabs**: Keys & pitch (key modes, magnet, life, glide, pitch/freq shift, bend, drone,
  gain) · Envelope · Timbre (tilt, odd/even, spread, shimmer, formant) · Filter & noise ·
  FX (Partial Delay, Rough Reverb) · Modulators (4 × shape/rate/depth/target, live meter).
  *Mutate sound* = small random nudge, *Dice sound* = randomise the whole synth half.
- **v0.3.1 chaos toys**: *Chord size / step* (Keys tab): one key plays a stack; in Scale mode the
  step counts SCALE DEGREES (great with Consonance scales). *Wander* (Timbre): every partial
  drifts on its own random path (± cents, rate). *Scramble* (Timbre): partials swap loudness.
- Everything above is a host parameter → automatable in FL (≈300 params, names say what they are).
