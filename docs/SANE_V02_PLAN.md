# Sane v0.2 — rework plan (after the user's first listen, 2026-10-18)

Status: PROPOSAL. Nothing built yet. The user asked for opinions on modulation before building it.

## 1. Why v0.1 felt the way it did (diagnosis, verified in the code)
| Symptom (user) | Cause | Fix in v0.2 |
|---|---|---|
| "Can't find basic delay settings (fine-tunable, Hz + BPM)" | The loop length was only ever set by the played key (loop = 1 / pitch). No TIME control existed. | LOOP TIME with units Note / Hz / ms / BPM + KEY TRACK 0–100 %. |
| "Base noise always audible unaffected" | The output tapped the raw delay line, so the injected burst came out unprocessed one period later. | Output tapped AFTER the loop processing + a separate DIRECT (dry hit) level, default low. |
| Freq shift: fine at 0 % and 100 %, thin tail in between | A dry/wet MIX inside a loop is applied on every pass: the dry and the shifted copy interfere (cancel) at most frequencies, so the loop loses energy fast. This is physics, not a bug, but it's a trap. | Inside the loop the slot knobs control INTENSITY (always fully wet). Dry/wet only exists for slots placed AFTER the loop. |
| "FX feel flat" | (1) Saturate and Fold were normalised to small-signal gain 1 and the loop level was low (~0.1–0.3), so they barely bent anything. (2) The same in-loop mix trap. (3) Timid knob ranges (e.g. shift range, disperse). (4) Every slot ran identically on all 8–16 lines, so their differences averaged out. | Calibrated internal level (the hit reaches ~1.0 inside the loop), DRIVE that really drives, wider ranges, per-line spread for some slots, and an IN LOOP / AFTER LOOP switch per slot. |
| "Can't remove FX" | The type picker had no "Off". | Remove (×) on every slot + "Off" in the picker + right-click menu. |
| "Hard to read" | 9–10 px text, low-contrast grey labels, thin lines, at 90 % window scale. | Bigger minimum text, stronger contrast, and a skin editor. |

## 2. New structure (signal flow, left to right)
```
EXCITER ──► EXCITER TONE ──► (strike) ──► LOOP ──────────────────────────► AFTER-LOOP ──► OUT
noise/lock   3-band EQ        position     core + TIME + feedback          slots, 5-band
envelope     (pre loop)       hardness     7 slots: IN LOOP or AFTER        dynamic EQ,
                                           loop                             width, level
                 └── DIRECT (dry hit) ────────────────────────────────────────►
```
- **Exciter:** Noise (Lock = the same noise every hit / Random + amount + seed), drawn envelope
  (attack, hold, decay, curve; draggable), 3-band tone (low / mid / high gain, mid frequency), level into the loop, DIRECT.
- **Loop:** core (Membrane / Partials / Delay); TIME = Note | Hz | ms | BPM (1/64 .. 4 bars, straight / triplet / dotted)
  + KEY TRACK (100 % = keys play pitches, 0 % = fixed delay time like a classic delay); FEEDBACK (decay, top = self-osc);
  DAMP; TENSION; COUPLING. Long times (BPM, up to ~2 s) only in the Delay core = rhythmic feedback drums / dub loops;
  Membrane and Partials stay pitched (up to ~60 ms).
- **Slots:** 7, interchangeable (drag), removable, each IN LOOP (every pass) or AFTER LOOP (once). New types: Dynamic EQ (5 bands), Gate, Comp.
- **5-band dynamic EQ:** reuse Seed's `shared/dsp/Eq.h` maths + EqView pattern, plus per-band dynamics (threshold / range).
  Available as a slot (in loop = bands shape every pass) AND as the fixed output EQ.
- **Init:** one INIT button = neutral patch (no randomness, no velocity/key effects, no slots, plain membrane).
  Middle-click on any control = its default (Seed already does this).
- **Hover help:** every control gets a one-line tooltip (what it does + where in the chain), plus a help line at the bottom.
- **Skin editor (temporary):** colour pickers for ground / ink / paper / display / text, a contrast slider and a text-size
  slider, saved in a user settings file (affects all projects), with a reset button.

## 3. Modulation — opinion (user asked: LFO mechanism like Seed?)
Recommended: **yes, the Seed system (drag a modulator chip onto ANY control), but with drum sources first.**
For drums the most important modulators are **per hit**, not LFOs:

| Source | Per voice? | Use |
|---|---|---|
| Velocity | yes | harder = brighter, more drive, centre strike |
| Key | yes | pitch-dependent tone in tuned mode |
| Random (new value each hit) | yes | humanising: every hit slightly different |
| Envelope 1–2 (drawn, per hit) | yes | e.g. freq shift that sweeps during the ring, filter that closes |
| LFO 1–4 (drawn, Hz/BPM, retrigger) | global or per voice | slow evolution, rhythmic movement |
| Mod wheel / aftertouch | global | live playing |

This replaces the fixed "velocity → X" bars of v0.1 (one consistent mechanism instead of special cases). Every
modulated control shows a ring for its range, like Seed. Cost: slot coefficients become per-voice when modulated per
voice (CPU goes up a little; measured before shipping). Alternative (simpler, less powerful): a fixed mod matrix (rows
source → target) without drag and drop. Recommendation: drag and drop (the user already knows it from Seed).

## 4. Order of work (each step testable by ear)
1. Engine fixes: output tap + DIRECT, in-loop intensity (no mix trap), drive calibration, remove slot, LOOP TIME + key track (+ BPM in the Delay core).
2. Exciter: lock / random, envelope, 3-band tone.
3. Init, tooltips, bigger text, skin editor.
4. 5-band dynamic EQ (slot + output).
5. Modulation system (after the user agrees with section 3).
A mockup 3 of the new layout comes before steps 2–5 if the user wants one.
