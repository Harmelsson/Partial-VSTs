# Seed — when notes are born and die while you play

_Design note, 2026-09-26. Status: **ideas, waiting for the user's pick.**_

## The problem
Seed v0.2 is meant to let the keyboard play the seed's *own scale*: the dips of its dissonance
curve (ArtHar's method 1). But everything is dynamic: scrub the seed while holding a chord and
- the dips **move** (easy: held notes glide with them),
- dips **disappear** (a valley flattens out) → a held note loses the note it was playing,
- new dips **appear** → notes are born that no key was holding,
- the **number** of notes per "octave" changes → every key above the anchor suddenly means a
  different scale degree.

So there are really two questions:
**(A) What does a *held* voice do when its note moves or dies?**
**(B) Which key means which note, when the note count keeps changing?**

## Ideas

### 1. Valley riders 🟡
A voice stops belonging to its key the moment it's struck: it belongs to a **valley** of the
dissonance curve, like a ball in a dip. The valley moves → the ball rides it. The valley flattens
out → the ball **rolls downhill into the neighbouring valley** (a glide whose speed follows the
slope). Two balls in the same valley → they merge into a unison, beating as they arrive.
Nothing ever cuts off. Physically intuitive, and it *is* the Sethares landscape made audible.

### 2. Strength = loudness ("fading ghosts") 🟡
Every note has a strength (how deep its dip is). Let the voice's volume follow it: a dying note
fades away as its valley flattens and is gone when the dip is gone; a newborn note under a held
key **fades in** as its valley deepens. A "Life" knob sets how much (0% = off, 100% = fully).
Combines with anything else here.

### 3. Mitosis 🔴
When a valley **splits** in two, the voice splits into two voices that glide apart; when two
valleys **merge**, their voices fuse. Hold a triad, scrub the seed, and it becomes a 5-note
cluster, then a dyad… The chord's *size* is played by the seed. The weirdest option.

### 4. Pinned keys / orphans 🟢
Conservative: a pressed key captures "degree 3 of this octave" and follows that note as it moves.
If the note dies, the voice freezes at its last pitch as an **orphan** (maybe with a slight
shimmer so you can hear it's lost) until you let go. New key presses use the new scale.

### 5. Magnet keyboard 🟡 (solves question B)
Keys never change meaning. As in v0.1's "keys follow seed", 12 keys span the seed's octave, evenly.
A **Magnet** knob pulls each key toward the nearest valley: 0% = plain stretched keys,
100% = every key snapped onto the scale (several keys may share a valley). Births and deaths
stop being events: they just change where keys get pulled. Held keys re-snap continuously and
glide, so a key whose valley dies slides to the next one, which is idea 1 for free.
Your FL piano-roll melodies keep their shape, and Magnet is a lovely knob to automate
(a small Hand of God).

### 6. Degree keyboard (the literal one) 🔴
Keys = scale degrees, wrapping every N keys. Fully "correct", but when N changes, every key
above the anchor shifts meaning mid-melody. That chaos could be a feature, with held voices
using 1–4 to survive the remap.

### Later, not v0.2
- **Gravity chords** (IDEAS I4 "Magnets"): the curve is computed against the *other held notes*
  instead of the sound itself, so chords slide into consonance with each other.
- **Crossfaded remaps**: during a remap a key plays its old and new meaning at once for ~0.5 s.

## Agent's recommendation
**v0.2 = 5 (Magnet keyboard) + 2 (Life knob)**, with 1 (valley riding) arising naturally from
held keys re-snapping. It stays playable from the normal piano roll, never cuts a note, and has
two expressive knobs. Then add **3 (Mitosis)** as a mode switch for when we want chaos, and
**6** as an alternative keyboard mode.

## Technical notes (for the agent)
- Needs a C++ copy of ArtHar's dissonance curve (`dissonance.rs`, Sethares constants) in
  `shared/xen/`. Cheap at 16 partials, but run it on a background thread whenever
  seed/rolloff/partials change and hand the result to the audio thread lock-free (atomic
  pointer swap to a pre-allocated curve buffer).
- Roughness depends on absolute Hz, so the curve needs a reference pitch (middle C, or the drone
  note). Pick one and show it in the UI.
- Snapping/riding is done per voice at control rate on the latest curve: gradient descent in
  cents with a glide-rate limit.
