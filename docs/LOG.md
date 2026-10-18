# Log (append-only, newest at bottom)

Entry format:
`## YYYY-MM-DD — session N — short title` then Done / Worked / Failed / User feedback / Next.

---

## 2026-09-26 — session 1 — project kickoff & workflow
**Context:** Repo had ArtHar notes (`xenharmonic_software_notes.md`, `questions.md`) and ArtHar
concept art. User: ArtHar moves to a separate standalone project; this repo is for experimental
JUCE VSTs for FL Studio. User = tester/vision director, agent = coder. Wants weird, unconventional
plugins. Sessions may end without warning → document after every task.

**Done:**
- Created `CLAUDE.md` (session protocol, project rules, planned layout).
- Created `docs/STATUS.md`, `docs/LOG.md`, `docs/LESSONS.md`, `docs/IDEAS.md` (13 seed ideas).
- Checked container: cmake/g++/clang/ninja present, JUCE repo reachable → local compile + headless
  DSP tests are possible; Windows VST3 must come from GitHub Actions.

**Decisions:**
- JUCE + CMake, VST3 target, Windows via CI.
- Shared non-bypassable safety stage in every plugin (carried over from ArtHar's hearing-safety rule).
- ArtHar docs kept as math reference only; their tech/design decisions don't apply.

**Next:** user picks first plugin (Seed vs Stretch Resonator) → Task 0 pipeline + that plugin.

## 2026-09-26 — session 1 — ArtHar calculator discovered & reviewed
**Context:** User revealed the ArtHar calculator already exists (Rust, v1.0, "advanced and usable",
no sound) and should be the base. It lives in private repo `Harmelsson/arthar`, which another
cloud session is modifying right now.

**Done:**
- Attached `Harmelsson/arthar` read-only, cloned snapshot `8bf556f`, read ARCHITECTURE, GLOSSARY,
  PROJECT_LOG and the calculator's stretch/loudness/dissonance/scale code.
- Wrote `docs/ARTHAR_REFERENCE.md` (summary + how to attach + integration decision).
- Updated CLAUDE.md and IDEAS (Seed = the calculator made playable).

**Decision (reversible, flagged to user):** port needed math to C++ in `shared/xen/`, use the Rust
crate as a test oracle — not FFI-linking. Reasons in ARTHAR_REFERENCE.md.

**Failed / blocked:** `cargo test` on arthar here fails — container has rustc 1.94.1, arthar needs
1.95. Fix when needed: `rustup update`.

**Next:** user answers: first plugin, merge policy, artifact download, and OK on port-to-C++.

## 2026-09-26 — session 1 — relationship clarified, Seed chosen
**User:** ArtHar and the VSTs are *completely different things*: ArtHar will never run VSTs; the
link is shared idea-origins and overlapping mechanism logic, nothing else. If something should
exist in both, the user asks for a total rewrite for that system. Arthar files are for reference
only, read only when the user points at a plugin there. "Copy what you can, that's the first step."
**User on Seed:** loves it. Core concept rule: *everything is dynamic* — changing the seed while
holding a note must be heard live; since notes' positions change, notes may be created/terminated
while playing → wants ideas for that problem. Seed is a synth → smoother, less intimidating UI,
gentler scrubbing/scrolling.
**Done:** CLAUDE.md + ARTHAR_REFERENCE.md rewritten to match (no oracle/linking, dynamic rule).

## 2026-09-26 — session 1 — Seed v0.1.0 + build pipeline
**Done:**
- CMake/JUCE 8.0.15 scaffold, shared safety stage + stretch math + gentle UI widgets.
- Seed v0.1.0: additive synth with rotator oscillators, control-rate (32 samples) smoothing
  (~30 ms), power-normalised voices, Nyquist and sub-audio fades, 16-voice stealing, drone,
  pitch bend ±2 st, keys-follow-seed (blends 100 c/key ↔ period/12 per key, so toggling glides).
- `tools/SeedTests`: 22 checks — all PASS on Linux (spectrum 1/n exact to 3 decimals, seed-2.5
  partial 2 at 1100 Hz, glide max-step 0.099 vs steady 0.078, worst-case peak = 0.794, state).
- UI snapshot checked via xvfb (`SeedTests --snapshot`); first layout had overlaps → fixed.
- GitHub Actions Windows workflow + docs/TESTING.md (install guide for the user).
- docs/SEED_NOTE_LIFE.md: 6 ideas for notes born/dying while held (user asked for ideas).
**Worked first try:** compile (only -Wfloat-equal warnings, fixed with juce::exactlyEqual).
**Not verified yet:** the Windows CI build itself, and anything in real FL Studio.

## 2026-09-26 — session 1 — user can't find the build → Releases
**User:** looked at the repo front page (main branch), saw no "green" anything, didn't know where
builds live. Also: on note birth/death — "we will test out different ideas, there is no harm in
creating variety" → implement several as selectable modes.
**Done:** CI publishes every successful build as a GitHub Release (front page → Releases box);
TESTING.md rewritten accordingly. Windows CI run #1: configure OK (4.5 min), build in progress.

## 2026-09-26 — session 1 — first green Windows build
- CI run #2 succeeded (compile + SeedTests on Windows). Sent the user the run link (artifact).
- Tried to download the artifact to send it directly: blocked by the container's proxy.
- User asked whether the agent could build Windows itself faster → no (Linux container,
  cross-compile untestable). Instead: docs-only pushes skip CI, superseded runs are cancelled.

## 2026-09-26 — session 1 — user tested v0.1 → v0.2 requests
**User feedback (build #2):** "holy moly, really nice for a first build!!!" but "looks kinda shit" →
redesign in ArtHar's dark style (sent an ArtHar v1.0 screenshot: near-black, off-white, thin frames,
letter-spaced caps section titles, number boxes with arrows, square-dot plots, scale table right).
Requests: velocity-dependent note volume; decay + sustain with an envelope in ArtHar style; the
dissonance-curve calculator on top. Vision: every synth gets the same/similar calculator on top;
calculator decides where notes/partials go; per synth some rules stay default, some modifiable,
some heavily modified. Synths will serve different purposes (plucks, drums, bells, whack sound
design). Seed = exploration for now.
**Plan v0.2.0:** C++ copy of dissonance.rs (curve, dips, strength, golden-section refine) in a
background worker; ArtHar-style shared UI kit (two-colour theme, per-digit ScrubBox, segmented
toggles, square lattice plots with easing + afterglow, scale table); ADSR + velocity sensitivity;
Fundamental + Min dip strength params. Keyboard does NOT play the found scale yet (that's the
note-life work, next).

## 2026-09-26 — session 1 — Seed v0.2.0 (ArtHar redesign + calculator + ADSR)
**Done:** C++ copy of ArtHar dissonance.rs (`shared/xen/Dissonance.h`, differences documented in
its header: pruned negligible pairs, grid ≤2000 points, |period| for seeds < 1) + background
worker; ArtHar-style UI kit in `shared/ui/`; Seed editor rebuilt (1160×800); new params
fundamental, minDip, decay, sustain, velocity (version hint 2); engine ADSR + velocity curve
(vel^(2·sens)) + fundamental as key-60 anchor in Seed-keys mode.
**Verified:** 35/35 checks. Curve copy matches ArtHar's pinned seed-2 notes to 0.1 c and seed 2.5's
strongest dip (927.93 c). 64 partials: ~90 ms per curve.
**Failures along the way (fixed):** `Font::getStringWidthFloat` gone in JUCE 8; a member named
`hitTest` hid Component::hitTest; sustain 0 never ended notes (smoothed value never reached 0 →
smoothing now snaps); "·" rendered as "Â·" (JUCE reads const char* as Latin-1); scale table clipped.
**Found, not a bug:** seed 0.5 (undertones) has only shallow dips (fifth ≈1.5%) → with the default
2% cut-off its scale is empty. Musically real: loud partials in the bass = smooth roughness.

## 2026-09-26 — session 1 — CI check-in results
- Run #1 green (took ~20 min: first run, no JUCE cache). #2 green. #3/#4 failed only at the Release
  step (403, see LESSONS). #5 green and published Release `build-5` (Seed v0.1.0).
  With the JUCE cache warm, configure takes ~30 s and the build ~10 min.
- Run #6 = Seed v0.2.0, check-in scheduled for 08:01 UTC.
- 08:01 check-in: run #6 (Seed v0.2.0) green on Windows incl. all 35 tests; Release `build-6`
  published. Handed to user.

## 2026-09-26 — session 1 — user loved v0.2.0, playground brief
**User:** "Holy shit... amazing work!!!" Minor bugs will be reported later. Brief for the next few
hours (user away): turn Seed into an experimental playground, chaotic on purpose; wants pitch
shifting, full automation of anything modifiable, presets (reuse a tuning with another sound),
ArtHar-style custom/disabled notes, custom partials + loudness, consonance method,
additive/subtractive tools, random modifiers, marked calculator modifications that fall back to
the calculator value when removed, synth separate from calculator, creative FX (delay on specific
partials, uncomfortable reverb). "~1.5 h of experimentation". Design: docs/PLAYGROUND.md.
Read ArtHar v1.4 (commit 4185c4b) consonance.rs + scale.rs note switching, with user's permission.

## 2026-09-26 — session 1 — Seed v0.3.0 playground built (user away)
**Done (see docs/PLAYGROUND.md):** consonance copy (ArtHar v1.4) · CalcModel (partial/note edits as
offsets with neutral values, custom slots) · calculator worker moved into the processor (message-
thread pump, try-lock handoff to audio) · ~300 params in SeedParams (TUNING vs SOUND groups) ·
engine rewrite (72 oscillators/voice, stereo spread, 4 key modes, glide, life, pitch + freq
shift incl. through-zero, tilt/odd-even/formant/shimmer, noise, per-voice SVF + filter env, two
partial-selective sends) · PartialDelay (ping-pong, spiral pitch per repeat) · RoughReverb (combs
on the scale + twins detuned to max Plomp-Levelt roughness, twins on the other ear) · 4 modulators
(sine/tri/saw/square/S&H/smooth/drift) · Dice/Mutate · presets (All/Tuning/Sound) · editable plots
+ scale table with orange "modified" markers · tabbed synth UI (1200×820).
**Verified:** 55/55 checks incl. consonance = ArtHar's 4:5:6:7 (seed 2) and stretched version
(2.5); edits mark + reset; muted/shifted/custom partials in the spectrum; Scale & Magnet keys;
pitch/freq shift; worst-case FX + 12× Dice stay finite and under the ceiling; mod moves the
calculator's seed; preset scopes.
**Worked first try:** the whole plugin compiled without errors after the rewrite; all new tests
passed on the first run (numbers inspected, they're meaningful).
**UI fixes after snapshots:** Dice tuning button squeezed; modulator rows wrapped.
**Known/unknown:** CPU in FL not measured yet (72 osc × 16 voices worst case); FX worst case
drives the limiter hard (safe, but loud) — levels may want taming after the user listens.
- v0.3.1 (same session): chaos toys — Chord (size/step; in Scale mode steps are scale degrees),
  partial Wander (shared random walk per partial), loudness Scramble. `--bench` CPU check added.
  Timbre tab relaid into two rows (first version overflowed the page). 59/59 checks.
- 09:05 check-in: CI run #9 (Seed v0.3.1) green on Windows incl. all 59 tests; Release `build-9`
  published (run #8 for v0.3.0 was cancelled by the newer push, as designed).

## 2026-09-26 — session 1 — user tested v0.3.1 (build 9) → v0.4 requests
**User:** "pretty decent for a first try". Questions: what are Seed-12, Magnet, Bend range, Chord
size/step, Mutate? Feedback:
- notes on the curve barely visible;
- MIDI: only 11 differentiated keys (12 with the next octave) of the usable scale, so a 20-note
  scale doesn't mean relearning the piano — and those 12 keys must be SELECTABLE;
- attack must go to 0 ms (even if it clicks); envelope times with 2 decimals of ms;
- dissonance/consonance settings should only show for the selected method;
- middle-click in empty space on the note plot sometimes creates a custom note (bug);
- custom partials like ArtHar (v1.15): tick boxes; they affect the calculation only when ticked;
- likes Dice sound/tuning, but wants "Dice settings" per page affecting only that page.
Read ArtHar v1.16 (83e50a9) partial tick feature with the user's permission.
**Mid-turn additions from user:** "Reset edits doesn't reset everything, the modulators stay";
"I'd like a visual representation of which note I'm pressing (see ArtHar)"; and "I'd like to revoke
my 12 key thing, I can do it manually by disabling and enabling notes. Sorry :D".
**Done (v0.4.0):** 12-keys mode built then removed again (default key mode = Scale instead);
played-note emphasis (ArtHar-style, engine reports held keys' note cents → UI matches within
15 c, 0.18 s fade); notes drawn much more visibly; env times 0 ms + 2 decimals (0 ms stage = one
sample); method-specific boxes; middle double-click bug fixed (left-only double-click adds);
Scale tick params p{n}scl / cp{k}scl + PartialsTable; per-page Dice/Mutate/Reset + Init; Reset
edits clears ticks + calculator-aimed mods. Window 1200×880. 66/66 checks.
**Answered in chat:** Seed-12, Magnet, Bend range, Chord size/step, Mutate.
- 14:44 check-in: CI run #10 (Seed v0.4.0) green incl. 66 tests; Release `build-10` published.
  (Note: the GitHub MCP release list sorts by tag name, so build-10 appears after build-5.)

## 2026-09-26 — session 1 — user tested v0.4.0 (build 10) → v0.5 requests
**User:** "feels really good to my ears". Feedback: release doesn't fade, it cuts off instantly
("something is in bad mojo"); show every scale type on the curve + the 12-EDO octave when on
Seed-12 (to see the deviation); FX "Picks" should be scrollable/continuous (between Odd and Even
= 50/50); loves the FX, wants more settings (panning…), more partial-FX ideas, an FX RACK with a
top-to-bottom chain whose slots can be swapped; compressors + other reverb types (experimental,
characteristic, simple); later learn about distortion types; everything automatable; Hz AND BPM
for every time-based setting; mod depth 2 more decimals (seed mod at 1% is drastic); import /
export scales, and maybe derive fitting partials for an imported scale (consonance / beating).
**Phase 1–2 done (v0.5.0 WIP):** release: headless release measured clean (smooth −60 dB fade), but
re-pressing a key re-used its still-releasing voice and cut the tail instantly → releasing voices
are no longer re-used (most likely what the user heard; ask to confirm). FX RACK: 5 slots top to
bottom, per slot type (Off / Partial Delay / Rough Reverb / Chirp Reverb (new, dispersive
all-pass spring tank) / Squash Comp (new, slope past ∞:1 into over-compression)), on, input
(Chain / Partials = per-slot partial bus from the engine), Picks = continuous morph between pick
rules (smoothed), mix, pan, width; ▲▼ swap slots (swaps parameter values). Delay gained
ping-pong, tone, BPM sync. BPM (host tempo) sync for modulators, shimmer, wander, delay.
Mod depth 3 decimals. Mod targets re-pointed to rack params (+ picks). Old sendA/ruleA/… params
removed (only in the user's test projects). 76/76 checks.
**Failure along the way:** a python replace that cut the old FX block also deleted the chaos-toys
params sitting after it → test crash (null param). Restored from git. Lesson added.

## Seed v0.5 WIP — Scala module (shared/xen/Scala.h)
- parseScl / writeScl / writeKbm + fitSeedToScale ("which seed makes these notes sit in dissonance valleys").
- Fit measure attempts: (1) note roughness vs timbre average → 12-EDO fitted seed 3.5 (wrong, FAILED);
  (2) valley depth at ±15/±35 c → 12-EDO 2.96 (still wrong, FAILED); (3) valley depth with the PERIOD
  weighted ×4 → 12-EDO 2.0000, Bohlen-Pierce 13 → 3.0000, JI major → 2.0000 (works). ~80 ms per fit.

## Seed v0.5.0 — Scala integration (Imported method, Import/Export/Fit buttons)
- Method "Imported" (3rd choice of Scale method): the worker draws the curve over the imported
  period and uses the file's notes; strength = valley depth (±25 c) of each note. Note edits apply.
- Import/Export via async FileChooser (export also writes .kbm). Fit seed searches 1.2–4.5 with up
  to 24 partials. The .scl text lives in apvts.state ("importedScl") and in presets (Tuning scope).
- Dice can pick "Imported" only when a scale was imported. Adding the 3rd choice shifts the
  normalised value of "Consonance" (old FL automation of Scale method would now point elsewhere).
- Tests: parse, reject garbage, BP import plays exactly (key 61 = degree 1), export round trip
  with a disabled note, .kbm, fit BP → 3.0000, 12-EDO → 2.0000, state restore. 86/86 pass.
- CI run #14 green on Windows (build + all tests) → Release `build-14` = Seed v0.5.0.

## 2026-09-30 — main was empty → new sessions saw no code
- User opened a fresh session for feedback; its agent reported the plugin "isn't in the repo". Cause:
  all work lived on `claude/amazing-planck-vf3au0`, `main` (where new sessions start) still had only
  the first upload (no CLAUDE.md either). User chose: agent updates `main` itself. Main fast-forwarded
  to this branch; CLAUDE.md now says to push to main after every task.

## 2026-09-30 — session 2 — user feedback batch 1 on v0.5.0 → v0.6.0
**User (first real try, verbatim-ish):** note lighting "not consistently showing, only when it feels
like it"; wants to **lock the scale** even when the seed changes (+ a popup warning before unlocking,
"the scale will change"); a **base key** (DAW's first MIDI note isn't the plugin's first when playing
on anything but C); **modulators** as a separate column on the right, 8 of them, listed top to bottom;
target chosen by **dragging the modulator onto a field**; one modulator → many fields; **Rate and
Division united** (only one shows when switching Hz/BPM; scrollable division, each with three versions
4/16, 4/16t, 4/16.). **FX**: more than 5 slots, scrollable rack (more FX feedback comes later).
**Filter & Noise tab can go**; instead a small dynamic visual **5-band EQ in the FX tab** (EQ at the
right, settings at the left: Q, steepness, band type; **key-trackable**). **Timbre**: same Rate/Division;
unsure what Chaos / Formant are; explain Shimmer (small reverb?). **Envelope**: "release is still just a
cutoff, it goes down and then instantly cuts at the selected length, I can even hear it clipping".
**Keys & Pitch**: what are Bend range and Life for? "12-EDO and 12-SEED should also show up on the
dissonance curve and the scale window, number scrollable 1–120 so it gives us a lot of notes."
(Session started with an empty `main`; the agent was pointed at CLAUDE.md + docs/STATUS.md.)

**Root causes found (measured, not guessed):**
- *Release*: `--release` probe showed −78 dB RMS then an instant drop to −120 dB at the set time: the
  voice was switched off when its envelope crossed 0.001 (−60 dB). Now it runs on to −100 dB (1e-5),
  i.e. −112 dBFS at a −12 dB gain: inaudible. Test: last audible block before the end < −94 dBFS.
- *Note lighting*: the UI only looked at the keys held at the instant of each 60 Hz refresh (notes
  shorter than a refresh never showed) and in 12-EDO / Seed-12 / Magnet a key's pitch is rarely
  within 15 c of a scale note (never lit). Fix: the engine latches every note-on into a ring
  (`uiHitCents`/`uiHitSeq`), the UI keeps a struck note lit ≥ 140 ms and always lights the nearest note.

**Done (v0.6.0):** see PLAYGROUND.md "v0.6.0 changes". Key design decisions:
- Modulation = LINKS (mod, parameter id, amount) kept in `apvts.state` ("modLinks" property, NOT
  parameters) and mirrored into a fixed lock-free table for the audio thread (`linkSlots`). Any float
  parameter can be a target (`ScrubBox` is a `DragAndDropTarget`; the editor is the drop handler).
  Modulators can modulate each other (one block delay). Old single `modNtarget` params are kept as
  legacy and converted to links on load (`absorbLegacyTargets`).
- New division parameters (`*beat`, 108 choices) instead of growing the old lists: old IDs keep their
  meaning. Old `*Div` params are unused legacy.
- The per-voice Filter + Noise were removed from the UI AND the processor stopped driving them (the
  engine code is dormant); their parameters stay (legacy IDs).
- EQ = Cytomic SVF sections (`shared/dsp/Eq.h`), per voice, before the FX rack; cut steepness = 1–4
  Butterworth sections; key track per band. Display response uses the same maths.
- Scale lock: snapshot of the raw scale (`lockedRaw`) saved in state ("lockedScale") and presets
  (Tuning part); the live curve keeps updating. Unlock asks first (`AlertWindow::showAsync`).
- EDO / Seed-EDO are calculator methods (notes placed at k·P/N, strength = valley depth like Imported).
  `kEditableDegrees` 32 → 120, `kMaxRawNotes` 64 → 128 (≈ 240 note-edit parameters).
- FX rack 5 → 12 slots; engine partial buses are now generic (only active buses are computed).
**Worked first try:** everything compiled without changes after the big rewrite; 126/126 checks.
**Failed / corrected on the way:** I first "migrated" the Scale-method parameter for old projects
assuming normalised values — but the state XML stores PLAIN values (a choice is its index), so
appending to the list needed no migration and my migration would have broken Imported. Removed;
lesson added. The drop-target test caught nothing; the UI itself (drag with a real mouse) is untested
headless — only the drop wiring and a screenshot were checked.

## 2026-10-13 — session 2 (cont.) — user tested v0.6.0 (build 16) → v0.7.0
**User (verbatim-ish):** "really good work so far". Feedback: draggable modulators should work on EVERYTHING that can be
edited with the mouse. "What I wanted with the 12-EDO / Seed-12 thing: if I change to those I still only see the
scale's notes on the curve window, not the selected ones — the visual should be totally dynamic. Now I notice the
'Scale method' does that, but it also overwrites the selectables in Keys&Pitch which isn't logical. Only the scale
method should remain, remove those selectables from Keys&Pitch." Modulators "exactly like PhasePlant or Serum": LFOs
drawable, fine-editable, selectable grid with Shift+drag snapping; trigger options MIDI / Audio (settable threshold) /
never; audio trigger on input, output or external sidechain. Clicking a listed modulator item (screenshot: "FX 4 squash
makeup") should highlight the setting, open its tab and give a slight orange glow. A "lock partials" like "lock scale".
Window can get too big: scale 25–200 %. "Instantly reactive even if my last click was inside the window? Now I have to click
the upper border or out to FL to play sound from my laptop's keyboard." FX feedback comes later.

**Done (v0.7.0):** see PLAYGROUND.md "v0.7.0 changes". Decisions / mechanisms:
- Any `RangedAudioParameter` (not the two locks) is a link target; `ModAware` mixin gives every control kind drop
  highlight + badge; plots/tables are DragAndDropTargets with callbacks that call `chooseAndLink` when an element has
  several parameters. `effUI()` (atomic per-parameter offsets published by the audio thread, explicitly zeroed when a
  link goes away) lets the message thread (calculator, note/partial edits) see modulated values.
- Key mode forced to Scale in the processor; Rig in tests now uses EDO 12 (+ settle) for "normal keys".
- LFO shape = `pvst::LfoShape` (≤ 64 points, per-segment bend), saved in state ("lfoShapes") and presets (lfoN attrs),
  copied to the audio thread through a try-lock; `Modulator::restart()` + one-shot. Triggers are evaluated per audio
  block (≈ 10 ms resolution). Audio sources: input / sidechain peaks of THIS block, output = previous block.
- New optional buses "Input" and "Sidechain" (disabled by default). JUCE lays input and output buses over the SAME
  channels, so with the main input off the sidechain is channels 0-1 (read before the output overwrites them).
- Lock partials = snapshot of (seed, rolloff, count) only; `CalculatorSettings::partialSeed()/…` and
  `EngineParams::partialSeed/partialRolloff` use it, everything else (period, Seed-EDO) keeps using the live seed.
- Window scale = `AudioProcessorEditor::setScaleFactor` + remembered percent in a text file. UNTESTED inside FL.
- Focus: Windows-only `SetFocus(GetAncestor(hwnd, GA_ROOT))` 120 ms after every mouse-up (skipped for text editors,
  modal windows, popup menus named "menu"). UNTESTED in FL — if keys still don't reach FL, next idea is to forward
  WM_KEYDOWN/UP to the root window or to map the typing keyboard to notes inside the editor.
**Worked first try:** the whole rewrite compiled; 5 old tests needed updating for the key-mode removal (Rig default).
**Failed on the way:** my sidechain test put the signal on channels 2-3 — JUCE shares channels between input and output
buses (see above); a first version of the link-chip click ran `onLocate` after a right-click removal (guard `ignoreUp`).


## 2026-10-17 — session 3 — drum plugin research (no code)
**Start:** branch `claude/vst-feedback-features-8keomg` held Seed v0.6.0 + v0.7.0, which were never pushed to
`main`. Fast-forwarded this session's branch to it; `main` gets it with this task's push.
**User (verbatim-ish):** new plugin idea while Seed isn't finished: drum sounds, "realistic and tribal-like ones
mainly, but an experimental project". Core = **feedback synthesis** ("the closest process to achieving realistic
drums that I know of"); the whole loop must run inside the one VST ("FL has some bad mojo against this"). Insert
"really fine delays and other types of FX that affect the whole sound or only partials within the feedback loop".
Research feedback synthesis (mainly for drums) and what makes drums drums (what resonates, pitch behaviour,
partials, harmonics?). Must be compatible with Seed and future plugins: scale and partial properties behave the
same and tuning is exchangeable ("coherent ecosystem"). Give multiple approaches to choose from. New visual design
to try: desaturated brown-desert yellow background, sharp blacks and whites for the important parts; every
changeable parameter also changeable in a graphical window (dynamic EQ, sound length, waveform in the background…).
After the concept, show a mockup first. "Don't build anything yet."
**Done:** web research → `docs/DRUM_RESEARCH.md`. Key findings: membranes are inharmonic (Bessel ratios
1, 1.59, 2.14, 2.30…); harmonic drums exist only by design (timpani via kettle and air, tabla via loaded head);
pitch drops with energy (tension modulation, ≈ proportional to energy → cheap model); two heads give mode pairs
and beating; Helmholtz bass in goblet drums; snare wires = nonlinear contact. A single feedback loop is harmonic
(string-like), so drum feedback needs dispersion, FDN coupling, per-partial loops, or nonlinearity. FFT inside
the loop rejected (latency per pass too long for drum pitches). Proposed architectures A (FDN membrane), B (partial
loops), C (loop rack), D (hybrid, recommended); exciters E1–E5; key modes K1–K3; mode sources M1–M3;
tuning exchange T1 .scl (exists), T2 copy/paste tuning, T3 live link, T4 MTS-ESP.
**Failed / limits:** WebFetch couldn't resolve several hosts (wikipedia, polarity.me, cornell), so some details
(e.g. the exact Avanzini equations) come from search summaries + prior knowledge, not full papers.
**Next:** user decisions → mockup in the desert palette.

## 2026-10-17 — session 3 — Sane decisions + mockup v1
**User decisions:** 1 D hybrid · 2 per choice 1 · 3 kit AND tuned ("both super useful") · 4 morph knob · 5 copy/paste
AND live link, links configurable ("maybe I want 2 synths linked, another not, or 2 together and 2 others together")
· 6 randomisable noise first, later audio as exciter ("me hitting my desk") · 7 the listed drums ok; future: full
drum kits and cinematic drums · 8 name **Sane** ("Insane in the membrane"). Visual direction "seems appealing,
but I can only say 'that's it' when I see it".
**Done:** interactive mockup on a design canvas (https://claude.ai/artifact/MbhbEy59oop9ECwuRGzZau), copy in
`docs/mockups/sane/`. Main window: exciter (noise burst display, dice), strike pad (click the head; mode levels
follow real Bessel J_m(j·r) for the membrane table), loop ring (Σ junction in/out, 7 insert slots on the ring,
core switch membrane/partials/delay, energy dots running round), modes display (level up, decay down, morph
slider moves modes between ◇ physical and ▲ seed positions, scale ticks from the linked tuning), hit timeline
(waveform, amplitude, pitch-drop curve, draggable length), selected-slot editor (dyn EQ over spectrum etc.),
keys strip (tuned = scale degrees on keys, kit = 16 pads). Second screen: 4×4 kit, pad "one key / key range",
link-group table (per instance OFF/A/B/C/D), what a group shares (seed / partials / scale / base key /
fundamental), group overview. Palette: ground #BDAE8B (+3 alternatives as tweaks), ink #121110, paper #F6F3EA;
Archivo condensed + IBM Plex Mono.

## 2026-10-17 — session 3 — user feedback on Sane mockup v1 → v2
**User (verbatim-ish):** "Overall look is great!" Displays: not black ("a little intimidating") → a little darker
desaturated brown than the base yellow. Exciter: re-randomise the noise burst on EVERY MIDI note, amount tunable and
modulated by velocity / keytrack. Strike: "great thing" → velocity sets position (quiet = rim, loud = centre); in
tuned mode keytrack too (higher = rim); amounts tweakable. Loop: the IN/OUT arrows overlap the ring arrows → remove.
Questions: does Strike only matter for Membrane? exact differences membrane/partials/delay? FX must be
interchangeable. "+ Slot" button confusing (no bigger editor). Modes: why morph physical↔seed? Wants morph between
"calculated (same as Seed's calculator) or inserted (tuning file)" and "linked (the common tuning)". Hit: two hit
boxes (title + play button) — play button not needed, unsure about the box; pitch-drop curve must be editable by
moving the curve. Link sharing options were only visible in the kit screen. No Tweaks button found. Kit = a menu that
extends the window on the right, drums listed and scrolled, clicking one edits it in the main window. Link conflicts:
what if two linked synths' parameters are automated at the same time? Design: "above and below each other, the
borders' corners should touch together" (aligned grid). Keyboard showing the tuned scale: loved — should light up on
press and release; **put that in Seed later too**. Wants another mockup before building.
**Done (mockup 2, same artifact URL, Kit artboard removed):** all of the above. Answers given in chat: Strike matters for
all three cores (membrane = where energy enters the FDN, partials = Bessel mode shapes, delay = comb on the hit);
core differences; slots are interchangeable (drag to swap, Type ▾); Modes morph endpoints are now selectable sources
(user's idea; Physical kept as an option because it is what makes drums sound like drums); link conflict options
(leader / last write / automation unlinks — recommended leader). Interpretation to confirm: "corners touch" = one
aligned grid where every border runs through.

## 2026-10-17 — session 3 — Sane v0.1.0 built ("let's hear it")
**User:** "I like the new layout more! I pick 1 [★ leader wins]… some things I'm unsure of I can only feedback once I
hear what they do. Great work so far, let's hear it. ^^"
**Done:** Sane v0.1.0 (`plugins/Sane/`): engine (3 cores as delay-loop banks, 9 insert types inside every loop, per-note
noise, strike vel/key, drawn pitch drop + energy tension, length, governor) + desert UI from mockup 2 + SaneTests.
**Worked first try:** pitch accuracy (LP phase compensation + Lagrange delay): every core within 0.1 cent.
**Failed / fixed on the way (all found by headless tests or WAV analysis, not by ear):**
1. Self-oscillation died: the loop locked onto DC (gain > 1 at 0 Hz); the output DC blocker then removed it → silence.
   → DC guard inside the loop (gain/phase compensated).
2. Governor too aggressive (min gain 0.2) killed self-oscillating loops → gentle, floor 0.6.
3. Default Membrane/Partials too quiet (-36/-45 dBFS) → per-core injection makeup + N^0.25 output normalisation.
4. Sub-bass rumble (8–26 Hz): any high-pass INSIDE a loop creates a "phantom" low mode. A stronger in-loop HP made it
   worse (and boosted harmonics when compensated). → gentle in-loop guard only + per-voice output high-pass outside the loop.
5. Membrane died far too fast: coupling `I - c·Householder` is lossy for 0 < c < 1 (mean direction × (1-2c) per pass).
   → lossless Givens rotations between neighbouring lines.
6. Bar table formula approximated mode 2 as 2.778 (should be 2.756) → exact beta values for the first modes.
Decisions: v0.1 leaves out kit drawer, link groups (rule ★ leader wins recorded), Seed scale keys, Pick/Dyn EQ/Air slots.
- 12:00 check-in: CI run #21 (main, Sane v0.1.0) green on Windows incl. SaneTests + SeedTests; Release `build-21` published.
  Build step took 26 min (was ~9 with Seed alone): two plugins with LTO, and a parallel branch run sharing the runner pool.

## 2026-10-18 — session 3 — user tested Sane v0.1.0 (build 21) → v0.2 plan
**User (verbatim-ish):** unsure how the feedback loop works; can't find basic settings of the main delay ("a fine tuneable
delay, both Hz and BPM"); unsure where each setting changes the sound → short description on hover. Noise is always random:
wants to choose constant vs random noise, an envelope for it and a 3-band filter on the noise before the loop. An INIT stage
(nothing randomised / velocity / keytrack) and middle click = back to base value. Filter may stay, but wants the dynamic
5-band EQ from the mockup. FX settings should connect to keytrack / velocity / random — "maybe an LFO modulator mechanism
like Seed? Don't build yet, just give opinions." Visually hard to read → maybe a temporary "skin maker" (own colours and
contrasts). Can't remove FX from the chain. Freq shift "works great on 100 % and 0 % but in between the sound has a very thin
tail". Base noise seems always audible unaffected → wants control. FX "feel flat… don't make the changes I'd expect".
"Overall, good direction, but needs some serious rework" — make every setting based on best knowledge of drums, feedback
synthesis, intuitive / practical / clean synth design.
**Diagnosis (verified in code):** no loop-time control (loop = 1/pitch only); output tapped the raw delay line (dry burst
audible); dry/wet mix INSIDE a loop interferes on every pass (thin tail); saturate/fold normalised to unity small-signal gain
with a low loop level (flat); picker had no Off. Plan + modulation opinion: `docs/SANE_V02_PLAN.md`.

## 2026-10-18 — session 3 — Sane v0.2.0 (rework step 1)
**User:** modulation = Seed-style; agrees: engine fixes now, mockup 3 for exciter / EQ / modulation; "modulations should go on the
bottom row above the keyboard". "Enjoy the task! ^^"
**Done:** see STATUS (loop time, direct, in/after-loop slots, remove, spread, calibration, help, init, middle-click, text).
**Found on the way (tests / probes, not by ear):**
1. Long BPM loops died between repeats: the "voice is quiet" check fired during the silence → wait > 1.5 loop lengths.
2. Calibration probe: loop-internal level was 0.05 (Partials) .. 0.6 (Membrane) → saturators did nothing. Now ~1.0 per core.
3. Partials: injecting the raw burst x 434 before the slots crushed every saturator → each line's share of the hit is
   band-passed first.
4. Switching a slot's type kept the old knob values (filter resonance became saturator asymmetry → pumping DC) → per-type defaults.
5. After-loop saturator then ran through the per-voice high-pass → spiky → after-loop slots are now last in the voice chain.
6. Strike position changed LOUDNESS by 6–7 dB (pluck comb mapped string-style; weights not energy-normalised) and cancelled
   velocity → RMS-normalised weights, burst energy independent of its length, drum-logic comb (Delay core only),
   default velocity → position -35 %. Partials keeps ~5 dB "rim is bigger" (constant-Q resonators) — accepted as character.
7. Soft-hit test measured the soft hit on top of the loud hit's tail, and used peak (misleading with many modes) → fresh
   instance + energy over 300 ms.
8. CPU: per-sample sin/cos in freq shift / ring mod / chorus → phasor + polynomial sine (small gain). Extreme patch ~83 %.
