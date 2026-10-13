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
  *Dice page / Mutate page / Reset page* act on the open tab only (v0.4).
- **v0.3.1 chaos toys**: *Chord size / step* (Keys tab): one key plays a stack; in Scale mode the
  step counts SCALE DEGREES (great with Consonance scales). *Wander* (Timbre): every partial
  drifts on its own random path (± cents, rate). *Scramble* (Timbre): partials swap loudness.
- Everything above is a host parameter → automatable in FL (≈300 params, names say what they are).

## v0.4.0 changes (user feedback on build 9)
- **Notes clearly visible** on the curve (solid guide lines, bigger squares), and **played notes
  light up** like ArtHar: the note's line turns bright, its square grows, its row in the Scale table
  lights up; released notes fade (0.18 s). Works in every key mode.
- **Key mode default = Scale** (keys = switched-on notes in order). The "12 selectable keys" idea
  was built and then withdrawn by the user ("I can do it by disabling/enabling notes").
- **Envelope**: attack / decay / release go down to **0 ms** and show **2 decimals**.
- **Method settings** only show for the chosen method (Min dip ↔ Tolerance + Min shared).
- **Bug fixed**: two quick middle-clicks no longer add a note/partial (only left double-click adds).
- **Scale ticks (ArtHar v1.15)**: new PARTIALS table (right, under Scale) with a *Scale* column.
  Ticked = the partial shapes the scale; unticked = sound only (drawn dotted). Hand-added partials
  start unticked; seed partials ticked; the fundamental always in. Mouse wheel scrolls both tables.
- **Dice / Mutate / Reset per page** (buttons right of the tabs act on the open page only).
  *Dice tuning* stays in the calculator. **Init** (header) = everything back to defaults.
- **Reset edits** now also clears Scale ticks and removes modulators aimed at calculator values
  (Seed, Fundamental, Rolloff, Min dip) — user: "Reset edits doesn't reset everything".

## v0.5.0 changes (user feedback on build 10)
- **Release fix**: re-pressing a key while its old note was still releasing stole that voice → the
  tail was cut instantly. Releasing voices are never re-used now (headless release was already smooth).
- **Curve shows where the keys sit** in every key mode (triangles under the curve, note names), and
  in Seed-12 / Magnet also the plain **12-EDO** positions as dashes for comparison.
- **FX rack** (FX tab): 5 slots, signal flows top → bottom; ▲▼ swap slots, square = on/off.
  Types: **Partial Delay**, **Rough Reverb** (kept), **Chirp Reverb** (new: a spring tank whose
  all-pass chain turns every click into a falling "pew"; Chirp 0 = plain tank, 1 = cartoon spring),
  **Squash Comp** (new: Slope 1 = off, 0.25 = 4:1, 0 = limiter, **below 0 = over-compression**: the
  louder you hit, the quieter it gets). Per slot: Input (*Chain* = what's above, *Partials* = only
  the picked partials of the synth), **Picks** as a continuous morph (drag / wheel: Odd→Even 50 %
  = half each), Mix, **Pan**, **Width**.
- **Hz or BPM** for every time: modulator rates, delay time, Shimmer and Wander rates each have a
  Sync switch + note division (host tempo; 120 BPM when the host doesn't say).
- **Mod depth** shows 3 decimals. New mod targets: FX slot 1/2 mix, picks, delay time/feedback, etc.
- **Scala import/export** (buttons bottom-right of the Calculator):
  - *Import .scl* loads any Scala scale → Scale method **Imported**. The notes are exactly the
    file's; your partials stay yours, the curve shows how rough your timbre is on those notes,
    and the Scale table's *Strength* = how deep a valley each note sits in. Note edits (on/off,
    move, custom notes) still work on top. Saved with the project and in presets (Tuning part).
  - *Export .scl* saves what the keyboard plays now (edits included, disabled notes left out)
    + a matching `.kbm` (key 60 = unison at the Fundamental) for other synths.
  - *Fit seed*: searches the seed whose stretched partials put the scale's notes into dissonance
    valleys (Sethares' "timbre follows tuning"). 12-EDO → 2.000, Bohlen-Pierce → 3.000.

## v0.6.0 changes (user feedback batch 1 on build 14)
- **Release**: the tail now fades all the way down (−100 dB) before the voice ends; no more cut.
- **Note lighting**: every struck note lights for ≥ 140 ms, the nearest scale note lights in every key
  mode (12-EDO / Seed-12 / Magnet too).
- **Lock scale** (Calculator row, next to Reset edits): freezes the current scale. Change the seed,
  partials or method: the locked notes stay (note edits still work on top), the dissonance curve keeps
  following the new sound so you can see how the locked notes sit against it. The curve title shows
  *SCALE LOCKED*. Clicking LOCKED asks "The scale will change" before unlocking (recalculates). The lock is
  saved with the project and with presets (Tuning part). Dice / Init: Dice never touches the lock, Init unlocks.
- **Base key** (Keys & pitch): the MIDI key that plays the scale's first note (default C4 = 60).
  Not used in the 12-EDO key mode (normal tuning).
- **Scale method: EDO and Seed-EDO** (+ *Steps* 1–120, scroll/drag the digits): N equal steps of 1200 c /
  of the seed's "octave". The notes appear on the curve and in the Scale table like any other (switch single notes
  off there). Strength = how deep a dissonance valley each step sits in.
- **Modulator column** (right side): 8 modulators. Drag the numbered chip onto any number box (one modulator can
  drive many boxes; several modulators can drive one). Linked boxes show the modulator numbers ("~1 3"), an orange
  bar = where the modulation pushes them. Under each modulator its link chips: drag up/down = amount
  (−100…+100 %), wheel = fine, right-/middle-click = remove. Right-click a linked box to remove a modulator from
  it. Modulators can modulate modulators (rate/depth). Dice/Mutate/Clear links at the top.
- **Hz or BPM in one control** (modulators, Shimmer, Wander, delay time): the Hz|BPM switch swaps the box for a
  scrollable division: 1/64, 1/32, 1/16…32/16, 48/16, 64/16, each straight ("4/16"), triplet ("4/16t") or dotted
  ("4/16."). Old division parameters are unused legacy; divisions restart at defaults in old projects.
- **FX tab**: 12 rack slots (scroll the list), slot settings scroll too; **5-band EQ** on the right: band picker,
  Type (Bell / Low shelf / High shelf / Low cut / High cut / Notch / Band-pass), Freq, Gain, Q, Steepness (cuts:
  12–48 dB/oct), **Key track** (0 % = fixed, 100 % = follows the note; the band sits at its Freq on the base key).
  Display: drag a node = frequency/gain (Q for cuts/notch/band-pass), wheel = Q, right-click/double-click =
  on/off; it shows the curve for the note you hold. The EQ runs per voice before the FX rack.
- **Filter & Noise tab removed** (per-voice filter + noise layer no longer driven; legacy parameters remain).
- **Tooltips** on the unclear controls (hover 0.6 s): Life, Bend range, Formant, Shimmer, Chaos, Base key, …
- Window: 1592 × 960 (scalable since v0.7).

### What the unclear controls do
- **Bend range**: how far the keyboard's pitch-bend wheel bends, in semitones (2 = ±2).
- **Life**: loudness follows each scale note's strength (valley depth in the dissonance curve): weak notes are quieter,
  deep ones full; 0 % = all equal. While holding keys and moving the seed, notes fade in/out as valleys appear/vanish.
- **Shimmer**: each partial's loudness pulses on its own (own speed/phase): a sparkling movement. Not a reverb.
- **Formant**: a fixed vowel-like bump (Hz, dB, width in octaves) on the partials' loudness: whichever partials land near
  it get louder/quieter, whatever note you play.
- **Chaos** (synth only): Wander = each partial drifts in pitch on its own random path; Scramble = partials swap loudness.

## v0.7.0 changes (user feedback batch 2 on build 16)
- **Modulate anything**: every control you can edit with the mouse takes a dragged modulator chip: number boxes
  (floats AND integers), switches, choices (Scale method…), the Hz|BPM switch and division, picks, envelope (asks
  which of A/D/S/R), EQ nodes (asks which setting), scale notes on the curve (position or on/off), the Scale table's
  on/off squares, partials in the plot (loudness or position), the Partials table ticks, FX rack rows (on-square or asks).
  Floats swing ±half their range at 100 % depth, switches/choices/integers use the full range (a switch only flips if it
  can cross the middle). Calculator values are modulated as the synth hears them. The two lock switches can't be modulated.
- **Key mode + Magnet removed from the UI.** The keyboard always plays the scale: choose the **Scale method**.
  EDO 12 (Fundamental C4, Base key C4) = normal piano; Seed-EDO 12 = the old Seed-12; Dissonance / Consonance /
  Imported / EDO any N. The curve and Scale table always show exactly the notes the keys play. (Legacy parameters stay.)
- **LFO editor** (top of the modulator column, for the selected modulator; click a row or its number): the curve is
  drawn like PhasePlant/Serum. Drag squares = points, double-click = add, right-click = delete, drag the small dots =
  bend a segment, **Shift+drag = snap to the grid** (X / Y division buttons), Presets menu (Sine, Triangle, Saw up/down,
  Square, Pulse, Ramp, Steps, Bounce, Invert, Reverse). The built-in Sine/Triangle/Saw/Square are shown and become drawn
  curves when touched; S&H / Smooth rnd / Drift are random and can't be drawn. A moving line shows the LFO position.
  Rows show a tiny picture of each shape.
- **LFO triggers** (per modulator): **Never** (free-running), **MIDI** (restart on every note-on), **Audio** (restart
  when the level of the **Input**, **Output** or external **Sidechain** rises through a **Threshold** in dB; it re-arms
  6 dB lower). **Loop / One-shot** (one cycle after each trigger, then hold). Input/Sidechain are optional plugin buses
  (off by default) — they only carry sound if the host connects them (not verified in FL).
- **Click a link chip** (e.g. "FX 4 squash makeup +100%"): the plugin opens that control's tab (FX slot / EQ band too),
  scrolls to it and lights it with an orange glow for a few seconds. Dragging the chip still changes the amount.
- **Lock partials** (calculator row, next to Lock scale): freezes the seed stretch, rolloff and count of the partial
  series. The seed then only sets the period / scale; hand edits still apply. Warning popup before unlocking. Saved with
  projects and presets (Tuning part), Dice never touches it. The partials plot shows PARTIALS LOCKED.
- **Window scale 25–200 %** (header, under the version): − / + / click for a list. Remembered between sessions
  (`Documents/Partial-VSTs/seed-ui.txt`). Uses JUCE's editor scale factor.
- **Keyboard focus (Windows)**: after a click inside the plugin the focus is handed back to FL's window (what clicking
  its border does), so FL's typing keyboard plays right away. Not done while typing into a text box or with a menu open.

## Recipes to start from (for curating)
1. **Consonant chord machine**: Method = Consonance · Key mode = Scale · Chord size 3, step 1 →
   every key plays three notes of a scale in which every pair fuses.
2. **Uncomfortable room**: Reverb send 60 % · Roughness 100 % · Decay 8 s → the tail rings on the
   scale's notes and beats between your ears. Roughness 0 % = the same room, eerily pure.
3. **Barber-pole partials**: Delay send 70 % · Picks = Odd · Spiral +700 c · Feedback 70 % → only
   the odd partials echo, each repeat a fifth higher.
4. **Living swarm**: Wander 60 c · Wander rate 0.3 Hz · Shimmer 40 % · Spread 100 %.
5. **Inharmonic bell pluck**: Seed 2.8 · Sustain 0 · Decay 1500 ms · Freq shift +37 Hz.
6. **Tuning that breathes**: Mod 1 = Smooth rnd → Seed, depth 10 %, rate 0.1 Hz · Key mode Scale
   · Glide 300 ms → held chords slide as the scale itself moves (notes may be born / die).
7. **Hand-built timbre**: Partials 8 · right-click partials 2 and 4 (mute) · double-click to add
   two custom partials · drag them around while a Drone plays.
9. **A timbre for a foreign scale**: Import .scl (e.g. Bohlen-Pierce, a gamelan pelog, 17-EDO)
   → *Fit seed* → play: the partials now line up with that scale's notes.
10. **Backwards breathing**: FX slot 1 = Squash Comp, Slope −0.5, Threshold −30 dB, Mix 100 % →
    loud chords duck themselves; release them and the tail swells.
8. **Save a tuning, swap the sound**: build a scale you like → Save as… → Dice sound a few
   times → set *Load: Tuning* and step through presets: the tuning comes back, the sound stays.
