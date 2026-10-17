# Plugin Idea Backlog

Status: `idea` → `next` → `building` → `testing` → `shipped` / `killed` (keep killed ones + reason).
Weirdness: 🟢 mostly conventional · 🟡 odd · 🔴 genuinely strange.
Risk = how likely the DSP is to be hard / unstable / need many iterations.
User: add ideas anywhere, any format — the agent will tidy them.

Core math shared by many ideas (from the ArtHar Calculator, see ARTHAR_REFERENCE.md):
stretched partials `f_n = f0 · n^S`, with `S = log2(seed)` (seed 2.0 = normal harmonics);
arithmetic partials `F, M·F, (2M−1)·F, …`; Sethares roughness/dissonance curve;
cross-note consonance `R ≈ (m/n)^S`.

---

## Instruments (MIDI in → sound)

| # | Name (working) | What it does | Weird | Risk | Status |
|---|---|---|---|---|---|
| I1 | **Seed** | The ArtHar Calculator made playable: additive synth, partials at `n^log2(seed)`, rolloff, drone mode, per-partial mute. Later: keyboard mapped to the calculator's found scale (dissonance-curve dips) instead of 12-EDO. Seed 2.0 must sound like a plain saw-ish tone (built-in ear check). | 🟡 | low | **testing** — v0.3.1 playground (see PLAYGROUND.md); user curates what stays |
| I2 | **Bellmaker** | Arithmetic partial series (`F, M·F, (2M−1)·F…`) — every register sounds different, bell/metallophone-like. | 🟡 | low | idea |
| I3 | **Chameleon** | Timbre bends to the chord: when you hold several notes, each voice's partials drift toward positions that make them consonant with the *other* held notes (Sethares in real time). Any chord "resolves" by changing timbre, not pitch. | 🔴 | med | idea |
| I4 | **Magnets** | The opposite: held notes' *pitches* glide toward nearby roughness minima relative to each other — chords settle into place by themselves, like the dancing XP squares. | 🔴 | med | idea |
| I5 | **Swarm** | Partials are particles with physics: springs to their "home" position, repulsion from dissonant neighbors, damping. Sound evolves on its own; knobs set gravity/temperature. | 🔴 | med | idea |
| I7 | **Mitosis** | Voices split/fuse as dissonance valleys split/merge (from SEED_NOTE_LIFE.md; may become a Seed mode). | 🔴 | med | idea |
| I6 | **Seed Morph** | XY pad / single "Hand of God" slider sliding continuously between two seeds (or two EDOs) live — tuning *and* timbre morph together as a playable gesture. | 🔴 | low | idea (could be a mode of I1) |
| I8 | **Sane** (drums) | Feedback-synthesis drums, realistic + tribal + experimental: exciter → feedback loop (FDN membrane / partial loops / fine delay) with FX inserts inside the loop on the whole sound or on picked partials; energy→pitch drop; Seed tuning ecosystem. See DRUM_RESEARCH.md. | 🔴 | med | **testing** — v0.1.0 (3 cores, 9 in-loop inserts) |

## Effects (audio in → audio out)

| # | Name (working) | What it does | Weird | Risk | Status |
|---|---|---|---|---|---|
| F7 | **Rough Reverb** | Combs on the scale + twins detuned to max roughness (built into Seed v0.3). Could become its own FX plugin. | 🔴 | low | in Seed |
| F8 | **Partial Delay** | Delay fed only by picked partials, spiral pitch per repeat (in Seed v0.3). | 🔴 | low | in Seed |
| F1 | **Stretch Resonator** | Bank of resonators tuned to a stretched partial series; MIDI or knob sets f0. Put it on drums/noise/voice → it rings like a bell that shouldn't exist. | 🟡 | low | idea — strong 2nd candidate |
| F2 | **Spectral Stretcher** | FFT, remap every frequency `f → f0·(f/f0)^S`. Stretches the harmonic series of *any* sample (vocals!). Artifacts welcome. | 🔴 | high | idea |
| F3 | **Partial Gate** | FFT; pass only energy that is consonant (or only dissonant) with a reference seed spectrum. | 🔴 | med | idea |
| F4 | **Roughness Amplifier** | Finds beating partial pairs in the input and exaggerates the beating (or cancels it). | 🔴 | high | idea |
| F5 | **Partial Delay** | Spectral delay: each partial band gets its own delay/feedback, set by its partial number or its dissonance score. Chords smear apart in time. | 🔴 | med | idea |
| F6 | **Dissonance Scope** | Analysis-only: live roughness curve / partial display of whatever is playing. Eye candy + teaching tool. | 🟡 | low | idea |

## Partial-based FX for Seed's rack (v0.5 proposals, not built yet — user to choose)
| # | Name | What it does | Weird | Status |
|---|---|---|---|---|
| R1 | **Partial Chorus** | each picked partial gets its own slow detune wobble (not the whole sound) | 🟡 | idea |
| R2 | **Partial Gate Sequencer** | BPM-synced steps switch picked partials on/off → rhythm made of timbre | 🔴 | idea |
| R3 | **Partial Auto-pan** | every partial circles the stereo field at its own rate | 🟡 | idea |
| R4 | **Spectral Phaser** | a notch sweeps over the partial INDEX (1 → N), not over Hz | 🔴 | idea |
| R5 | **Beat Maker** | detunes partial pairs so they beat exactly at 1/4, 1/8… of the host tempo | 🔴 | idea |
| R6 | **Partial Freeze** | hold/sustain chosen partials after the note ends | 🟡 | idea |
| R7 | **Tuned Distortion** | "distortion" built additively: new partials placed at stretched positions (intermodulation that respects the seed) | 🔴 | idea (user wants to learn distortion types first) |

## Notes / constraints
- FL Studio handles MIDI-effect plugins awkwardly → put scale/keyboard mapping *inside* each
  instrument rather than as a separate MIDI-FX plugin.
- Tuning ideas (N-note scales from a seed) can live inside instruments as a "keyboard map" mode.
- **Seed backlog (user 2026-10-17):** bottom keyboard strip like Sane's mockup — keys show the tuned scale degrees and light up on press / fade on release.
