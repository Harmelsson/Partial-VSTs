# Drum plugin: research + design options (no code yet)

_2026-10-17, session 3. The user asked for research only. The user picks options, then we do a visual mockup, then we build._

The brief (user, 2026-10-17): a drum-focused plugin, realistic and tribal-sounding but experimental, built on
**feedback synthesis**. The whole loop runs inside ONE VST (FL's routing can't do it). Very fine delays
and other FX go **inside the feedback loop**, acting on the whole sound or on **single partials**. Scale and
partial behaviour must match Seed so the plugins form one tuning ecosystem. A new look: desaturated
desert-yellow background with sharp black and white. Every parameter also shown and editable in a picture.

---

## Part 1 — What makes a drum sound like a drum

### 1.1 Three ingredients: exciter → resonators → radiation
Every drum is a short **hit** (exciter) feeding one or more **resonators** (head, second head, air,
shell, wires) that are coupled together and lose energy at different speeds.

| Stage | What happens physically | What the ear hears |
|---|---|---|
| Contact (0–5 ms) | Stick/hand/mallet presses the head. Contact time ≈ 0.5 ms (hard stick) to 5 ms (soft mallet, palm). Harder or faster hits = **shorter contact = brighter** (felt and skin are nonlinear springs). | Click, slap, thud: the "what hit it" part. |
| Burst (5–50 ms) | Many modes ring at once. Pitch is at its **highest** (see 1.4). Snare wires rattle. | The body of the hit, noisy and dense. |
| Ring (50 ms–2 s) | High modes are gone, a few low modes remain, pitch settles down. | The "note" of the drum, its tail. |

**Strike position matters.** A hit in the centre mostly excites the round, ring-shaped modes (heavy thump,
short). A hit near the edge excites the "rocking" modes (more ring, more overtones, more "tone"). A mode
whose node line passes through the strike point is not excited at all. A hand drummer's bass / tone / slap
is mostly strike position plus how long the hand stays on the skin.

### 1.2 The head (membrane) is INHARMONIC
A plain round skin has modes at these ratios to its lowest mode (from the zeros of Bessel functions):

`1.00 · 1.59 · 2.14 · 2.30 · 2.65 · 2.92 · 3.16 · 3.50 · 3.60 · 3.65 · 4.06 · 4.15 · 4.23 …`

Compare a string: `1 · 2 · 3 · 4`. The membrane's modes are **dense and don't line up**, and that is
most of what makes a drum sound like a drum and not like a note. Other resonators in percussion:

| Resonator | Mode ratios | Sound |
|---|---|---|
| Ideal membrane | 1, 1.59, 2.14, 2.30, 2.65… | Generic drum / tom |
| Timpani (kettle + air) | Rocking modes pulled to ≈ 1 : 1.5 : 2 : 2.5 : 3, the thump mode dies fast | Pitched drum (on purpose) |
| Tabla / mridangam (black loaded patch) | ≈ 1 : 2 : 3 : 4 : 5 (Raman, 1920) | **Harmonic** drum, sings |
| Wood/metal bar (slit drum, marimba, log drum) | 1 : 2.76 : 5.40 : 8.93 | Woody "tok" |
| Plates / cymbals / gongs | Very dense, and the energy moves upward over time (nonlinear) | Shimmer, wash, crash |
| Air cavity (Helmholtz: djembe, darbuka, udu, cajón) | A single low resonance, ≈ 60–120 Hz | The deep "doum" / bass tone |

So **are harmonics present?** Usually no. The exceptions are drums that were built to be harmonic:
timpani (shaped kettle plus air), tabla and mridangam (loaded head), and some tuned kick heads.
Harmonic drums sound pitched and "vocal". Inharmonic ones sound like noise with a pitch centre.

### 1.3 What shapes the real sound beyond the ideal skin
- **Air loading** lowers every mode, the lowest ones the most. That is why real drums don't match
  Bessel exactly.
- **Two heads + trapped air** (toms, snare, kick): every mode splits into a **pair** (heads moving
  together vs. against each other) at slightly different pitches. The tuning between top and bottom
  head decides sustain, beating, and how the pitch bends. Same pitch = long sustain. A small gap =
  "wah"-like beating. A big gap = short and dry.
- **Shell / cavity**: a Helmholtz resonance (an air spring) gives the bass of goblet drums. On a djembe
  the bass (≈ 65–80 Hz) comes mostly from the air in the body, the tone (≈ 300–420 Hz) and slap
  (≈ 700–1000 Hz) from the skin. Shell wood adds faint fixed resonances (colour that doesn't follow
  pitch).
- **Snare wires** rattle against the bottom head. This is a **nonlinear** contact: noise bursts whose
  loudness follows the head's motion. Without wires a snare "sounds sort of like a bass drum"
  (Nord Modular book). Wires also buzz *sympathetically* when another drum hits their pitch.
- **Jingles / rattles** (tambourine, daf, shakers): many tiny random collisions, best done as
  particles (Perry Cook's PhISM model), not as resonators.

### 1.4 How the pitch behaves
- **Pitch drops after the hit (tension modulation).** A big swing stretches the skin, raises its
  tension and so its pitch. As the energy decays the pitch slides down, roughly **exponentially**. A
  harder hit gives a bigger and longer slide. Loose, low-tuned heads slide most (deep toms, floor toms,
  kicks, taiko). Research shortcut (Avanzini, Marogna and Bank, DAFx 2010 / JASA 2012): the slow
  tension change is about **proportional to the energy still in the drum**, so the model is
  "pitch = base pitch × (1 + k × energy)". That costs almost nothing and catches the important part
  of the glide.
- **Player-controlled pitch.** Talking drum (squeezing the laces gives speech-like glides), tabla
  bayan (the heel slides on the skin), udu (a hand on the side hole bends the air resonance), and
  pressing a frame drum from behind. The pitch moves continuously while the drum is still ringing.
- **Perceived pitch is often weak.** The ear picks the loudest long-ringing mode or a "virtual"
  pitch. Many kicks have no stable pitch at all, only a frequency region.
- **Mode-dependent decay.** Higher modes die faster (losses grow with frequency). On single-headed
  drums the lowest round mode radiates so well that it dies first, which is why the thump is short
  and the overtones ring longer.

### 1.5 "Tribal" families worth covering (a world map of exciters and resonators)
| Family | Examples | Key physics to model |
|---|---|---|
| Frame drums | bodhrán, tar, daf, riq | single thin head, hand damping from behind, jingles |
| Goblet drums | djembe, darbuka/doumbek | Helmholtz bass + head; bass / tone / slap strokes |
| Loaded heads | tabla, mridangam | harmonic modes, heel pitch-slide (bayan) |
| Hourglass | talking drum (dùndún) | large live pitch glides |
| Big barrel drums | taiko, dhol, surdo | thick head, slow huge pitch drop, long ring |
| Pot / aerophone | udu, ghatam | air resonance + bendable "boing" |
| Wood | slit/log drum, cajón, woodblock | bar modes 1 : 2.76 : 5.40, Helmholtz (cajón) |
| Shakers / rattles | shekere, caxixi | particle collisions (stochastic) |

---

## Part 2 — Feedback synthesis: what it is and why it suits drums

### 2.1 The idea
No oscillators. A signal goes around a **loop** (delay → processing → back to the start). You inject a
short kick of energy (the exciter) and the loop **rings** at the frequencies its delays and filters
allow. Physics works the same way: waves running across a skin and bouncing off the rim are a
feedback loop. That is why feedback patches sound so physical.

- Loop gain < 1 → the sound decays (a drum hit).
- Loop gain = 1 → it rings forever (a drone).
- Loop gain > 1 → it explodes unless something nonlinear (a saturator) holds it. That is the wild,
  self-oscillating "feedback" sound people mean when they say feedback synthesis.

**The catch:** one loop of length T rings at T's harmonics (f, 2f, 3f… = Karplus-Strong = a
**string**, not a drum). Everything below is about **breaking that harmonicity** in good-sounding ways.

### 2.2 Why it must live inside one plugin (confirms the user's call)
Feedback through FL's mixer goes through at least one audio buffer (typically 256–1024 samples, 5–20 ms).
A drum mode at 200 Hz needs a loop of 5 ms or less, and fine detail needs fractions of a millisecond. Inside
a plugin the loop runs **sample by sample**: delays down to a single sample, fractional delays,
and per-sample nonlinearities. That only works in-plugin.

### 2.3 The family of feedback structures (the building blocks)
| # | Structure | How it becomes drum-like | Character | CPU |
|---|---|---|---|---|
| L1 | **Single delay loop** (Karplus-Strong) | **Drum variant**: flip the sign of the sample at random with probability b. At b = 0.5 the pitch is gone and it sounds like a noisy snare or tom (Karplus and Strong, 1983). | Lo-fi, crunchy, very "feedback" | tiny |
| L2 | **Dispersive loop** (all-pass filters inside the loop) | Frequency-dependent delay pushes partials apart, so the harmonic series becomes **stretched**. This is the same thing as Seed's `n^S`! | Springy, metallic, stiff-string → bar | small |
| L3 | **Feedback Delay Network (FDN)**: 4–16 short delays mixed by a matrix | Many coupled loops give a dense, inharmonic set of modes, like a 2D skin or a body. Originally a reverb design; with 0.3–15 ms delays it is a resonant **body**. | Realistic drums, wood, shells, rooms | small–medium |
| L4 | **Waveguide mesh** (a 2D grid of tiny loops) | Literally a skin: Bessel-like modes come out for free, and strike position and mic position are real places on the grid. | Most "physical", toms and snares | medium–high |
| L5 | **Partial loops** (one tiny loop per partial: a 2-pole resonator is a 1-sample feedback loop) | Each partial is placed exactly (Bessel, tabla, Seed's `n^S`, imported). A coupling matrix lets energy flow between partials. | Precise, tunable, clean → organic with coupling | small (64 partials × 16 voices is fine) |
| L6 | **Nonlinear feedback** (saturator in the loop, feedback AM, **energy → pitch**, passive nonlinear all-pass) | Energy-dependent pitch gives the tom glide. A saturator gives self-oscillation and grit. A *passive* nonlinear all-pass (Pierce and Van Duyne, 1997) spreads energy upward **without adding energy**: gong/cymbal shimmer, rattles, stable by design. | From realistic glides to chaos | small |
| L7 | **Coupled loops** (two or more structures feeding each other weakly) | top head ↔ bottom head ↔ air (Helmholtz) ↔ snare wires | Realism through interaction, beating, sympathetic buzz | small |

### 2.4 FX inside the loop: what each one does when it goes round 100× per second
An effect in a loop is applied again on every pass, so mild settings have huge effects.
| Insert | Inside a loop it becomes… |
|---|---|
| Very fine delay (0.02–20 ms) | the **pitch** of a mode (delay = 1/f) |
| All-pass / phaser | **inharmonicity** (stretching the partials), "spring" chirps |
| Low-pass / EQ / dynamic EQ | **frequency-dependent decay** (highs die faster = natural), tone over time |
| Saturator / folder | brightness that grows with loudness, self-oscillation, grit |
| Pitch / frequency shifter | spirals: every pass moves the partials (rising or falling hits, "laser" toms) |
| Gate / compressor | shaped decays, "held" rings, gated-reverb drums |
| Modulated delay (chorus) | detuned mode pairs = two-head beating |
| Energy follower → delay time | **tension modulation** = realistic pitch drop |
| Band split / partial split | different decay, glide or FX **per partial** (the user's core idea) |

### 2.5 How to apply FX to "only some partials" inside the loop — 4 approaches
| # | Approach | How it works | Pros | Cons |
|---|---|---|---|---|
| P1 | **Band split** | Crossovers split the loop signal into 3–8 bands, each with its own insert chain, then sum. | Cheap, robust, any loop type | Bands ≠ exact partials |
| P2 | **Partial picker (tracking band-passes)** | Narrow band-passes centred on the actual partial frequencies, which follow pitch and seed live (like Seed's "Picks"); picked partials go through the insert, the rest bypass. | Real "per-partial" feel, follows glides | Works best when partials are known (L5, L2) |
| P3 | **One loop per partial** (L5) | Each partial *is* its own loop, so inserts are per partial by construction (each gets its own fine delay, decay, drive, glide). | Exact, visual, very weird potential | Less "accidental" richness than one big loop |
| P4 | **FFT/spectral in the loop** | — | — | **Rejected for the core:** an FFT adds 10–40 ms of latency per pass, far too slow for drum pitches. Possible only as a slow "spectral echo" outside the body. |

### 2.6 Stability and hearing safety in a feedback instrument
Feedback is the easiest way in DSP to make something blow up. The plan, whichever architecture wins:
1. A **soft limiter inside the loop** (passive saturator), so the loop can't grow without bound even at gain > 1.
2. An **energy governor**: it measures loop energy and gently pulls the feedback down if a note rings louder than the hit.
3. All parameter changes smoothed (fractional delays with interpolation, so delay-time sweeps glide instead of clicking).
4. The shared **SafetyStage** at the output as always (NaN scrub → DC block → ceiling), never bypassable.
5. Headless tests: render every preset at max velocity / max feedback, check level, no NaN, decay time.

---

## Part 3 — Architecture options for the plugin (user decides)

### A. "Membrane Network" — physical-first
```
exciter ─► [ FDN body: 4–16 coupled fine delays + per-line filters ] ─► out
                ▲      ↕ coupling             ↕ coupling
          energy→pitch  [ 2nd head loop ]     [ air / Helmholtz loop ] ─ [ snare/rattle ]
```
- Sounds: the most **realistic** toms, snares, djembe, taiko, cajón.
- Partial FX: bands (P1) only. Partials aren't exact here; they emerge.
- Ecosystem: the network is tuned so its lowest modes land on the scale's pitch; Seed's partials only "colour" it.
- Weirdness: medium (matrix morphing, mesh size, impossible materials).

### B. "Partial Loops" — ecosystem-first
```
exciter ─► [ N partial loops at Seed/Bessel/tabla positions ] ⇄ coupling matrix ─► [ shared body FDN (small) ] ─► out
               each partial: fine delay · decay · drive · glide · insert slot
```
- Sounds: precise, tunable drums (tabla, timpani, tuned toms, impossible "seed drums"). Coupling and a
  small body add the organic mess back.
- Partial FX: exact (P3). Every partial is an object you see and grab.
- Ecosystem: **perfect.** Positions come straight from the shared calculator (`n^S`, edits, custom
  partials, .scl). A Seed timbre becomes a drum.
- Weirdness: high (per-partial delays and glides, partials that spiral, partials that feed each other).
- Risk: can sound "synthetic" until coupling, noise and the body are tuned. Needs headless A/B renders.

### C. "Loop Rack" — experiment-first (closest to the user's description)
```
exciter ─► ┌─────────────── feedback loop ───────────────┐ ─► out
           │ slot 1 ► slot 2 ► … slot 8 ► (gain) ─────────┘
           slots: fine delay · all-pass · filter/dyn EQ · saturator · shifter · gate · partial picker · band split · body
```
- Sounds: anything from KS-drum crunch to realistic (with the right slots) to totally new.
- Partial FX: a "Partial Picker" or "Band Split" slot gives sub-chains (P1/P2).
- Ecosystem: the main delay (and picker frequencies) follow the scale; dispersion can be set *by seed*.
- Weirdness: highest. Realism depends on presets that we build and test.
- Risk: easy to make mush. Needs good defaults and the safety governor.

### D. Hybrid (my recommendation)
**C's rack wrapped around a selectable core:** the loop's "body" slot can be **FDN membrane (A)**,
**Partial loops (B)** or a **plain fine delay (KS)**. Around it: exciter section, inserts that target
*whole loop* or *picked partials*, an optional 2nd loop (second head / air / snare) and energy→pitch.
That gives realism (A), the ecosystem (B) and freedom (C) in one design. Build order would be:
exciter + one core + 4 insert types first (testable by ear quickly), then more cores and coupling.

---

## Part 4 — Exciters (how the loop gets hit)
| Option | Notes |
|---|---|
| E1 Synth impulses | click, noise burst, "mallet" pulse whose width shrinks with velocity (= brighter when harder). Strike position as a filter/comb on the exciter. |
| E2 Built-in recorded exciters | short stick, hand, finger, brush, rod, beater hits (we'd make or source CC0 ones). Very realistic attacks. |
| E3 Audio input / sidechain | play the drum with any audio (beatbox, a loop, Seed!). The inputs from Seed v0.7 are already implemented. |
| E4 Continuous exciters | friction/rub (cuica, bowed drum), breath noise (udu, ghatam). Feedback loops respond beautifully. |
| E5 Hand actions over time | **press** (damping during the ring = more loop loss), **slide** (tension = live pitch bend), mute on note-off. Mapped to velocity, aftertouch, mod wheel, pitch wheel, Seed-style modulators. |

## Part 5 — Keyboard + tuning ecosystem with Seed

### 5.1 What "same behaviour" means (from Seed's code)
Seed's partials: `f_n = f0 · n^S`, `S = log2(seed)`, rolloff `1/n^r`, hand edits stored as offsets,
custom partials. Scale: Dissonance / Consonance / Imported .scl / EDO / Seed-EDO, base key, period =
`1200·log2(seed)` cents. All this lives in `shared/xen/` and is reused by the drum plugin unchanged.
The same calculator gives the same scale and the same partials.

### 5.2 How keys map to drums (pick one, or both as modes)
- **K1 Tuned mode:** every key plays the same drum, pitched to the scale's notes (melodic toms, tabla
  sets, talking-drum melodies, xen percussion).
- **K2 Kit mode:** keys = different drums (FL-friendly drum programming). Each pad's pitch still snaps
  to a chosen scale degree.
- **K3 Both:** a kit of N pads, each pad tuned and playable melodically over its own key range.

### 5.3 Where the drum's modes come from
- **M1 Physical tables:** membrane (Bessel), timpani, tabla (harmonic), bar, plate. The scale only sets the pitch.
- **M2 Seed series:** modes at `n^S`. Seed 2.0 = a harmonic drum (tabla-like), other seeds = alien drums
  whose overtones fit the scale (Sethares: "timbre matches tuning").
- **M3 Morph:** one knob slides each mode from its physical position to its Seed position (membrane ↔
  seed drum), live while ringing.

### 5.4 How plugins exchange tuning (four levels, pick any)
| # | Method | Live? | Effort | Notes |
|---|---|---|---|---|
| T1 | **.scl/.kbm files** (Seed already exports and imports) | no | done | universal, works with any synth |
| T2 | **Tuning preset / clipboard** ("Copy tuning" in Seed → "Paste tuning" in Drum): seed + partial edits + scale | no | small | carries partials too, which .scl can't |
| T3 | **Live link channel**: a "Link A/B/C/D" selector; one instance publishes, the others follow live (shared memory between our DLLs in FL's process) | **yes** | medium | honours "everything is dynamic": move Seed's seed and the drums retune live. FL's plugin bridging (separate process) needs a fallback, so this must be tested in FL. |
| T4 | **MTS-ESP** (ODDSound standard; open client library + free libMTS) | yes | medium | Seed or Drum as a tuning *master* retunes many third-party synths. Carries note pitches only, not partials. Could join T3 later. |

---

## Part 6 — UI direction (for the mockup step, after the user chooses)
- Palette: desaturated desert-yellow ground (≈ `#BDAE8B`, kept slightly greyish), **pure black** for
  structure, lines and important values, **white** for active, highlighted and playing things. One theme file
  (`shared/ui/Theme.h` is already "two colours + mixes"), so Seed stays dark and Drum can be the desert
  theme, and the user can compare.
- Every parameter has a picture and is draggable there:
  - **Loop diagram** (the heart): the signal ring with its slots drawn on it. Energy flows visibly
    around the ring; per-slot mini graphs (filter curve, delay length as arc length, drive curve).
  - **Mode display**: partials as vertical lines (height = level, length of the fade tail = decay),
    drag to tune, glide arrows showing tension pitch drop; Seed-style scale ticks under it.
  - **Hit timeline**: the rendered waveform of the last hit in the background; decay, length, pitch
    curve and choke drawn on top and draggable.
  - **Dynamic EQ inside the loop** drawn over the live spectrum.
  - **Strike pad**: a round drum head; click position = strike position, and mode shapes light up.
- Decision 1 below decides which pictures are the main ones.

---

## Decisions for the user
1. **Architecture**: A Membrane Network / B Partial Loops / C Loop Rack / **D Hybrid (recommended)**.
2. **Partial FX method** inside the loop: P1 bands / P2 picker / P3 per-partial loops (depends on 1).
3. **Keys**: K1 tuned / K2 kit / K3 both.
4. **Mode source**: M1 physical / M2 seed / **M3 morph (recommended, includes both)**.
5. **Tuning exchange**: T1 is already there; add T2 / T3 live link / T4 MTS-ESP? (Order suggestion: T2 → T3 → T4.)
6. **Exciters for v1**: E1 synth (recommended start) + which of E2–E5?
7. **First drums to aim at** (for by-ear testing): e.g. tom, djembe, tabla, taiko, frame drum, snare?
8. **Name** (working): *Hide* (drum skin, desert), *Loop Drum*, *Membrane*, *Skin*, or the user's own.

## Sources
- Tension modulation: Avanzini, Marogna and Bank, [DAFx 2010 paper](https://dafx.de/paper-archive/details/FF0VCsWcetDbuSbluOmOiQ), [Bank DAFx 2009 (strings)](https://home.mit.bme.hu/~bank/publist/dafx09/index.html)
- Membrane modes and real drums: [HyperPhysics circular membrane](https://hyperphysics.gsu.edu/hbase/Music/cirmem.html), [Chalmers thesis comparing FDTD/delay/modal/mass-spring](https://odr.chalmers.se/items/afae68a3-f8d8-40ad-a139-b52b34450dd6), [Sound On Sound: synthesizing snare](https://www.soundonsound.com/techniques/synthesizing-drums-snare-drum), [SOS bass drum](https://www.soundonsound.com/techniques/synthesizing-drums-bass-drum), [Nord Modular percussion chapter](https://cim.mcgill.ca/~clark/nordmodularbook/nm_percussion.html)
- Timpani: [Munroe notes (1 : 1.5 : 2 : 2.5 : 3)](https://gauss.dartmouth.edu/~m5f10/Munroe2004notes_review_instrument_physical_models.pdf), [Davis, Purdue](https://docs.lib.purdue.edu/dissertations/AAI8825522), [Mills (Rossing), NIU](https://huskiecommons.lib.niu.edu/allgraduate-thesesdissertations/6350)
- Two-head coupling: [Rossing/Zhao, NIU thesis](https://huskiecommons.lib.niu.edu/allgraduate-thesesdissertations/974/), [snare modes](https://labs.sonicfield.org/library/modes-of-vibration-and-sound-radiation-from-a-snare-drum), [ViennaTalk 2015](https://viennatalk2015.mdw.ac.at/proceedings/ViennaTalk2015_submission_43.pdf)
- Tabla: [Eigenspectra of Indian drums](https://arxiv.org/pdf/0809.1320), [Acoustics of the Indian drum](https://arxiv.org/pdf/math-ph/0001030), [Raman 1920](https://repository.ias.ac.in/52555)
- Djembe: [UBC wiki](https://wiki.ubc.ca/Djembe); talking drum: [Frontiers 2021](https://www.frontiersin.org/articles/10.3389/fcomm.2021.652690/pdf); udu: [BYU](https://physics.byu.edu/docs/publication/1551)
- Gongs/cymbals: [Legge and Fletcher 1989](https://www.phys.unsw.edu.au/music/people/publications/Leggeetal1989.pdf), [Chaigne/Touzé/Thomas](https://researchportal.ip-paris.fr/en/publications/nonlinear-vibrations-and-chaos-in-gongs-and-cymbals/), [tapered cymbals JASA 2019](https://perso.ensta-paris.fr/~touze/PDF/2019_BaoCT_TaperedCymbals_JASA.pdf), [passive nonlinear filter patent](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/5703313), [cymbal synthesis tutorial](https://mcld.co.uk/cymbalsynthesis/)
- Feedback structures: [Karplus-Strong (J. O. Smith)](https://ccrma.stanford.edu/~jos/pasp/Karplus_Strong_Algorithms.html), [feedback AM, EURASIP 2011](https://asp-eurasipjournals.springeropen.com/articles/10.1155/2011/434378), [Five variations on a feedback theme](https://mural.maynoothuniversity.ie/id/eprint/2333/), [filter pinging](https://sdiy.info/wiki/Ping_a_filter), [Max *perc (allpass-loop percussion)](https://cycling74.com/projects/max-for-live-*perc-%E2%80%94-percussion-synth), [delay-loop percussion patent](https://patents.google.com/patent/US5223653)
- Mallets: [force pulse synthesis](https://ccrma.stanford.edu/~jos/pasp/Force_Pulse_Synthesis.html), [Papetti impact model](https://avanzini.di.unimi.it/downloads/publications/papetti_taslp11.pdf)
- Tuning sync: [simple-mts-main](https://github.com/baconpaul/simple-mts-main), [MTS-ESP Mini](https://www.kvraudio.com/product/mts-esp-mini-by-oddsound/details)

## User decisions (2026-10-17)
1. Architecture: **D Hybrid** (loop rack around a switchable core: FDN membrane / partial loops / fine delay).
2. Partial FX method: follows from D (band split + partial picker slots; per-partial inserts in the partial-loops core).
3. Keys: **both** tuned mode and kit mode ("both super useful").
4. Mode source: **morph knob** (physical ↔ seed).
5. Tuning exchange: **copy/paste AND live link**. Links must be configurable groups: e.g. 2 plugins linked, another
   not, or 2+2 in separate groups → link channels (A/B/C/D/off) per instance.
6. Exciter v1: **randomisable noise**. Later: audio input as exciter ("me hitting my desk" → processable source).
7. First drums: tom, djembe, tabla, taiko, frame drum, snare. Future: full drum kits and cinematic drums.
8. Name: **Sane** ("Insane in the membrane").
Visual direction: appealing, but the user decides only after seeing it → mockup next.
