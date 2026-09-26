# Xenharmonic Software — Open Questions (v4)

> Nothing gets deleted, ever. Answered waves live in RESOLVED, summarized but permanent. My own voice — answers, pushback, recommendations — is always in <span style="color:#c026d3">**magenta**</span>.
>
> This round added a lot of real design, not just answers — see the new **DISCUSSION** section at the very bottom for my take on the big-picture stuff (Hand of God, architecture doc format, the few things I think need a small tweak before they become permanent).

---

## RESOLVED — Wave 1: Core Questions

**GUI vs. math-first:** Math always comes first and must work correctly before GUI is built around it. GUI is secondary but never trivial — concrete visual ideas exist (beautiful, minimalistic) and should inform later UI work.

**"V1 done" means:**
- Working math for basic artificial-harmonic calculation.
- Scrubbable "seed" input (e.g. 2.5 stretches harmonics *and* the perceived octave to that ratio instead of 2:1).
- Real-time scale-note calculation via two selectable methods: dissonance curve, harmonic consonance.
- Consonance output is the software's **opinion, not fixed** — every note property is meant to be manually overridable later. (Now fully specified — see RESOLVED Wave 5/7 below.)
- Sheet/table display pre-GUI.
- Export to .scl, .tun, .fnv — confirmed v1 scope.

**Sound in v1:** No. Math correctness first, sound built on top later.

**OS:** Windows. **No dedicated soundcard** — ASIO or WASAPI only, for now (this matters once Wave 6/audio starts, not yet).

**Rust toolchain:** `rustup`, official installer, VS C++ Build Tools accepted alongside it. Chat copy-paste is fine until the codebase gets big enough that Claude Code becomes worth the switch.

**Bug reporting:** Mix of plain description / screen recording / screenshot, as fits the issue.

**Session explanation level:** Full explanations wanted, can live in code comments or MD files rather than chat.

**Session size:** Task-dependent — repetitive/derivative work can go in one big step; genuinely new work stays careful and incremental.

**Scaffold-now vs. lean:** Stay lean, one exception (never hard-code a partial as a bare float with zero room for a future source/origin tag). Confirmed correct approach — see DISCUSSION at bottom for the one addition this round's answers require (stable parameter addressing, for Hand of God).

**Deadline:** None — fully open-ended, quality and enjoyment over speed. <span style="color:#c026d3">Explicit long-horizon framing added this round: this is a decades-scale project ("until my retirement"), which changes how much weight architecture-quality carries relative to short-term speed — see DISCUSSION.</span>

---

## RESOLVED — Wave 2: Sequencing & Session Logistics

**Rust scaffold timing:** A dedicated setup session, labeled **v0.0**, when work actually begins.

**Architecture doc:** Required from session 1, no exceptions — past projects suffered badly without one.

**Mid-session stopping:** Always prefer a clean, tested checkpoint over a messier "finished" feature.

**Flagging expensive architectural decisions:** Yes, always, even if it slows things down. Precision/practicality/quality over speed, explicitly — <span style="color:#c026d3">this is the same long-horizon framing as Wave 1's deadline answer, worth treating as a standing project value, not a one-off preference.</span>

**Session-end summaries:** Yes, "what changed / what to test," every session, dynamically enough that a future session/agent can pick up context fast.

**CHANGELOG vs. single file:** Undecided, pending token-cost question — <span style="color:#c026d3">answered in DISCUSSION below.</span>

---

## RESOLVED — Wave 2.5: Modulator Vision

**Calculator vs. Track separation (core architecture, confirmed):** The Calculator is pure math — it never makes sound. It produces numbers (note/partial frequencies, positions) that are shared globally across every Track in a session by default. A Track can opt out via a **"Deviate Calculator"** toggle, which spawns a smaller Track-local Calculator that the Track uses instead of the Global one, ignoring Global entirely.

**Calculator modes (illustrative list, explicitly open-ended, "no limits"):** harmonic-stretch vs. arithmetic-multiply partials; dissonance-curve vs. harmonic-consonance note placement; variable note count; octave ratio tied to or independent from the seed number; optional octave-less operation; optional frequency-range-locked operation (e.g. 20–4000 Hz sandbox). More calculation methods expected over time — this list is a floor, not a ceiling.

**Track internals:** Phase-Plant/Bitwig-grid-like — horizontal rows of stackable synths/generators, each with its own modifier chain in series, all consuming Calculator numbers.

**Two distinct modulator types (core distinction, confirmed):**
1. **Calculator-number modulators** — change the *shared* number itself. If this moves note 2 from 260→280Hz, every Track using that Calculator sees the change.
2. **Track-copy modulators** — change only the copy of the numbers already handed to one specific Track. Lets one Track (e.g. a violin) subtly drift a partial for realism without affecting any other Track using the same Calculator.

**Hybrid / "Hand of God" modulator (confirmed, explicitly "far future," full vision recorded verbatim rather than summarized, since precision matters here):** A dynamic, visual, X/Z-style controller capable of driving *any* parameter in the system — note positions, per-note harmonics, synth/FX parameters — from a single mouse drag, in real time, recordable as automation. Explicit target example: sliding continuously between something like 12-EDO and 17-EDO live, mid-song, as a playable technique, not just a settings screen. Stated goal: "total control over every quality a sound has." <span style="color:#c026d3">This is the one idea in the whole vision with a direct, non-optional architectural cost today — see DISCUSSION.</span>

**Modulator brainstorm (non-exhaustive, all confirmed as directionally good, expect more over time):** consonance-based partial gates/filters; chord morphers (Hand-of-God-style, not preset-to-preset); partial-targeted delay/reverb/compressor/distortion via a "Partial Send" mechanism; live spectral analysers that extract a recorded sample's harmonic series and derive its dissonance-curve position / octave-stretch automatically; sample partial-stretchers (move/amplify a sample's extracted partials — flagged as especially useful for vocals/unusual timbres); "partial extenders" (insert N interpolated partials between existing ones by ratio).

**Terminology:** No fixed vocabulary yet for modulator sub-categories (LFO-like / macro-like / send-targeting / FX-like / synth-like) — expected to be defined collaboratively as the system grows. <span style="color:#c026d3">Folded into the future glossary — see Wave 10.</span>

**Plugin model:** In-house only, not VST-compatible. Explicit reasoning: third-party format constraints would limit the system in ways that would remove the enjoyment of building it.

---

## RESOLVED — Wave 3: V1 Calculator Specifics

**Partial count:** Scrubbable, globally and per-note. Per-note override is explicitly an in-system-only feature — acknowledged that this means some session states will not be losslessly exportable to .scl/.tun (those formats are "primitive" relative to what this system will hold internally). <span style="color:#c026d3">Correct and expected — noted formally in DISCUSSION as an explicit non-goal (full internal-state export) rather than a gap.</span>

**Per-partial display:** Both Hz and cents-from-fundamental shown side by side to start; trim later based on what's actually useful in practice.

**Sub-fundamental / negative stretch factors:** Open question, deferred pending discussion — <span style="color:#c026d3">answered in DISCUSSION below.</span>

**Fundamental frequency bounds:** Open question, deferred pending discussion — <span style="color:#c026d3">answered in DISCUSSION below.</span>

**Decimal precision:** No single global default — different settings need wildly different precision (the seed/"magic number" may need up to ~10 decimals of audible sensitivity; other settings need integers only, no decimals at all). Precision is a per-field property, not an app-wide constant.

**Partials above ~20kHz:** Excluded by default pending a concrete use case — <span style="color:#c026d3">one exists, see DISCUSSION.</span>

**Base-number ↔ stretch-factor alternate input:** Open question, needs a plainer re-explanation — <span style="color:#c026d3">re-explained in DISCUSSION below.</span>

**Reset-to-default:** Yes, an initial preset is wanted. This also opened up preset-saving as a topic in general — confirmed that presets will carry substantial data per save (see next item).

**Seed presets:** Confirmed — seed values should be manually saveable/loadable as presets, not just free-scrubbed.

**Dissonance-curve vs. consonance method selection:** Toggle between the two (not simultaneous display) — they're separate states, and more calculation methods are expected to join the toggle over time.

---

## RESOLVED — Wave 3.5: File Export — scl / tun / fnv

**Priority:** Roughly equal across all three; fine to build them in separate sessions rather than all at once.

**Test references:** Will help test directly; the Sevish scale-file tools/site can be used as an additional reference/validation source.

**Save location:** Fixed output folder acceptable while testing if a full save dialog is nontrivial to build; proper file-save dialog preferred if it's not much extra effort.

**Embedded metadata:** Confirmed yes — exported files should carry seed/stretch-factor/generation-method as a comment where the format allows it.

---

## RESOLVED — Wave 4: Per-Digit Scrubbing UI

**Drag direction:** Up increases the value (not inverted). Scroll-wheel direction matches (scroll up = increase).

**Fine-mode trigger:** Shift, held while dragging.

**Scroll-wheel adjustment:** Confirmed yes, in addition to drag.

**Type-exact-value fallback:** Confirmed yes, via double-click on the value.

**Carry/borrow ripple:** Real time, resolves live while dragging, not just on release.

**Decimal-columns-visible setting:** Wants per-field precision (not one global setting) — proposed behavior: trailing zeros collapse/hide automatically (box shrinks), non-zero trailing digits expand the box to show fully; alternative/additional idea floated: hover-to-reveal full precision even when currently-zero. <span style="color:#c026d3">Both are good, complementary rather than competing — my take is in DISCUSSION.</span>

**Drag sensitivity:** Start at 10px = one step, tune by feel once running; a user-adjustable sensitivity setting is wanted eventually, not necessarily in v1.

**Full visual/UI description (recorded in full, not summarized — this is a real spec, not a preference):**
- **Color:** Minimal palette. Two primary colors set the whole base look (one background, one for borders/text/graphical info/contrast). Secondary/accent colors used sparingly and *meaningfully* — e.g. optional per-note/per-partial coloring, optional octave-boundary marking on the custom scale, optional color-coding tied to a plugin's function (e.g. a distortion module visually reddening as it's driven harder). Multiple view-modes possible later (e.g. consonance-strength or spectral-amplitude shown via color).
- **Motion:** Every animation fluid and smooth, minimalist — never showy or aggressive. Specific reference image given for the dissonance-curve visualization: old Windows-XP-logo-style squares dancing/settling into place, evoking an "oldschool Matrix/Cyberpunk... quantum, geometry, simplicity" feel.
- **Layout (first pass, explicitly unfinished/exploratory):**
  - Top: **Global Calculator** — a narrow horizontal bar showing typeable base numbers (e.g. the seed), collapsible via a dropdown arrow into the full Calculator GUI beneath it. A modifier row lives beneath the Global Calculator for modulators that alter its shared numbers.
  - Per-Track **"Deviate Calculator"** toggle spawns a smaller Track-local Calculator (same look, lighter GUI) that the Track uses instead of Global.
  - Below that: a Track view resembling Phase Plant — horizontal rows for stacking synths/generators, each with a serial modifier chain governing how that Track turns Calculator numbers into sound.
  - Right side: a vertical **hybrid-modulator / macro** panel — controls capable of touching Global or per-Track parameters (this is where Hand-of-God-style controls would live).
  - Bottom: a **Mixer**, deliberately closer to a conventional mixer (volume, compression, sidechain, buses) and deliberately *less* deeply modulatable than individual Tracks, to protect workflow — heavy modulation belongs upstream, on the Track, before signal reaches the Mixer.
  - MIDI-mapping controls for a Calculator (Global or Deviated) are expected to live near the top or left edge of its window — left edge is explicitly unused/available space right now.
  - Explicitly unresolved: where pattern/clip arrangement lives (Ableton-style split view floated as a possibility), where a piano roll fits, and a strong positive reference to Reaper's always-on **Global Sampler** (persistent background recording, drag-and-drop into a track any time) as a workflow feature worth learning from.

---

## RESOLVED — Wave 5: Consonance / Dissonance Calculations

**Integer-pair bound / cents tolerance ("~28"):** Not a real preference — traced back to an example number Claude had used in an earlier explanation, mistakenly treated as a stated requirement. No standing preference exists; defaults should be proposed and adjusted by ear/eye when this is actually built.

**Separate modules vs. one Calculator with toggles:** Correction to the original framing — dissonance-curve and consonance are not meant to be separate *modules*, they're two toggleable calculation modes inside one Calculator, both producing the Calculator's "opinion" about where notes belong.

**6-note base-2.5 scale as a regression test:** Not applicable as originally framed — those hand-calculated numbers were Claude's own worked example from an earlier conversation, not user-supplied reference data, so there's nothing there to regression-test against. General principle affirmed instead: once the Calculator's core math for a given method is correct, it should not silently drift — a new calculation method can be *added*, or an actual bug can be *fixed*, but existing correct math shouldn't change incidentally as a side effect of unrelated work.

**Default partials-per-note for consonance scanning:** Same correction — "16" was Claude's example number, not a stated preference. Confirmed default: 16, configurable — chosen now as a real decision, not inherited from a misread example.

**Consonance score tunability:** v1 ships the simple/default version only. The deeper mechanism wanted is explicit and now fully specified: **the Calculator states a value (e.g. "Note 1 = x"), and any override is stored as a persistent delta relative to that computed value** (e.g. "Note 1 = x + 2%"). If the seed or method later changes and recomputes x, the override re-applies as "new x + 2%" — the offset survives underlying recalculation until manually cleared. <span style="color:#c026d3">This is a clean, standard "computed value + sticky delta" pattern — confirmed as the actual override-layer design, not just a v1-adjacent idea. See RESOLVED Wave 7 below, which is now essentially answered by this.</span>

---

## RESOLVED — Wave 7: Generator/Override Layer

**Lock vs. relative-offset overrides:** Resolved by the Wave 5 answer above — overrides are relative deltas on top of a live-recalculating base value, not hard locks.

**Overrides saveable/loadable:** Confirmed yes — and generalized: *anything* that can be moved/adjusted in the system should be saveable and loadable as part of a preset, not just overrides specifically.

**Generator blending question:** Marked as possibly no-longer-relevant given how the override model got clarified above — <span style="color:#c026d3">confirmed obsolete, folded into DISCUSSION rather than left as an open question.</span>

---

## RESOLVED — Wave 6: Audio Engine (still deprioritized — no sound yet — but design intent now recorded)

**Solo/mute individual partials:** Confirmed strongly wanted, whenever audio arrives.

**Partial amplitude/envelope shaping:** Not applicable to "v1" specifically since v1 has no audio at all — confirmed wanted once any audio layer exists.

**Always-on drone mode:** Confirmed strongly wanted — instant, continuous audibility of changes while scrubbing, not strictly note-on/note-off.

**Polyphony:** Monophonic first, specifically for hearing-safety reasons while levels/calibration are being dialed in — polyphony is the explicit next step once the math and gain-staging are trustworthy.

**MIDI input timing:** Wanted from very early on — laptop keyboard is planned as an immediate "audition these settings" input method, not deferred to a later modulator-era feature.

**Audio hardware constraints:** No dedicated audio interface currently — ASIO or WASAPI only, expected to remain true for roughly a month or two.

---

## RESOLVED — Wave 8: Multi-Modulator & Shared Data Model

**Modulator #1 — different synthesis approach or same-engine-more-knobs:** Expected to start with simple/conventional synths for early testing, then expand into deep experimentation over time — largely already covered by the Wave 2.5 modulator vision above.

**Switching vs. simultaneous/layered modulators:** Covered by the Track/Calculator architecture in Wave 2.5 — multiple Tracks, each with their own modulator chains, running simultaneously.

**First FX module priority:** Reframed — before EQ or any other FX, **a non-optional, non-disable-able safety Limiter** is the actual first priority, specifically to prevent hearing/equipment damage once any audio output exists. EQ remains a good early module after that.

**Multiple simultaneous tuning systems:** Covered by the Global/Deviate-Calculator-per-Track architecture in Wave 2.5 — each Track can independently opt into its own tuning context.

---

## RESOLVED — Wave 9: MIDI / Visual Keyboard

**Current controller:** None yet — considering an Exquis specifically for its moddability. Long-term goal: support multiple distinct internal "virtual controller" profiles (own note-naming, own layout/design) that can be switched between, not just one fixed keyboard mapping.

**Remap timing:** Instant, on any scale/stretch change — no manual "apply" step.

**Hardware to test against:** None yet, expected within a few months.

---

## RESOLVED — Wave 10: Misc / Catch-All

**Version numbering:** Formal (v0.1, v0.2, etc.) confirmed wanted.

**UI color/visual preference:** Fully covered by the Wave 4 visual description above.

**Session/window-state persistence:** Wanted fairly early — floated as a good candidate for the first thing tackled right after v1 ships, going into a "v2" phase.

**UI-feel reference software:** No additional references beyond Surge/Harmor/Phase Plant, already known.

**Glossary:** Wanted, and explicitly extended — should include both standard xenharmonic terminology *and* project-specific coined terms as they get created (e.g. "Global Calculator," "Deviate Calculator," "Hand of God," future modulator-category names).

**Anything missed:** None flagged — positive feedback on the original question set.

---

## STILL OPEN — Wave 2: two remaining sub-items

Q: CHANGELOG as a separate file from day one, or fold into the single notes file for now (pending the token-cost question, answered in DISCUSSION below) — confirm once you've read that answer?
A:

---

## DISCUSSION — Claude's take, for us to actually talk through

<span style="color:#c026d3">

**1. CHANGELOG vs. one file — direct answer to your token-cost question.**
Two 10-line files vs. one 20-line file costs basically the same in tokens either way — the file *sizes* are what matter, not the file *count*, and splitting/merging doesn't meaningfully change total size. So there's no token-efficiency reason to delay splitting. The actual reason to wait isn't cost, it's clutter: a CHANGELOG is only useful once there's enough history that "what changed recently" and "what's the current state" are different questions. Right now they're the same question. My recommendation: **one file for now** (call it `PROJECT_LOG.md` or similar), split into a separate `CHANGELOG.md` the first time a session-end summary would otherwise push the combined file past a size where skimming it gets annoying — probably somewhere around v0.3–v0.5, a judgment call I'll flag when it comes up rather than a fixed rule now.

**2. Hand of God needs one real architectural commitment today, not "later."**
For a future control to grab *any* parameter in the system and automate it, every parameter needs a stable, addressable identity from the moment it's created — not retrofitted once Hand of God is actually being built. Concretely: instead of a Track's synth just holding a plain `frequency: f64` field, it needs to be identifiable by a path like `track_3.synth_1.partial_7.frequency` that a future modulator can reference, store, and animate. This is a small naming/structure discipline now (basically: give everything a stable ID as it's created) versus a full rewrite of the data model later if it's skipped. I'll build this in from the Calculator's very first version — it costs almost nothing today and is the single most load-bearing decision in the whole file for the multi-year vision you described.

**3. Sub-fundamental / negative stretch factors — direct answer to your question.**
Your instinct is right that a naive negative stretch just relabels which partial you call "the fundamental" — not musically new by itself. But there's one genuinely interesting case: **an asymmetric stretch**, where positive-direction partials get one stretch treatment and an independently-controllable "downward" set gets a different (or mirrored) one. That's not "negative harmonics," it's two coupled harmonic series meeting at a shared root — closer to how some bell/gong-like or inharmonic real-world timbres actually behave, and to "undertone series" experiments some xenharmonic composers already use deliberately. Worth keeping as a real Calculator mode later (not v1), not worth building generic unbounded negative-X support for its own sake.

**4. Fundamental frequency bounds / "is absurd ever interesting" — direct answer.**
Two genuine cases where going past normal audible range is still useful, even though the fundamental itself won't be heard: (a) **sub-audible fundamentals used purely as a math anchor** — you never hear 2Hz, but partials 40 and up land back in audible range with unusual, non-integer-related spacing, which is a legitimate way to generate strange partial sets; (b) **ultrasonic fundamentals for FM/AM-style modulation math** — some synthesis techniques use an inaudible carrier purely for the *interaction* it produces with an audible signal, not to be heard directly. Given that, my recommendation: don't hard-clamp the range, but do visually flag (dim, not hide) frequencies outside typical hearing, so it's clear what's audible without blocking legitimate use of the inaudible end for math purposes.

**5. Partials above ~20kHz — the "any use case?" question, answered.**
Yes, one concrete case: if the Calculator ever supports the asymmetric/undertone stretch idea from point 3, or arbitrary non-harmonic partial spacing, an upper partial can drift above 20kHz even when the fundamental and most of the series are perfectly normal and audible — excluding it entirely would silently hide a real, correct result of the math. My recommendation lands close to your instinct though: compute and store them (so nothing's silently lost), but don't render them in the default partial list — same "dim, don't hide" treatment as point 4, available on request rather than shown by default.

**6. Base-number ↔ stretch-factor alternate input — re-explained plainly.**
This was a confusingly-worded question, sorry. Plain version: you already said entering a "seed" like 2.5 drives both the harmonic stretch *and* the octave ratio. The question is just: should there ever be a *second* way to arrive at that same 2.5, by typing some other related number and having the software back-calculate the seed from it? Given nothing else in your answers describes such a second number, I think the honest answer is **this doesn't apply yet** — it only becomes a real question once a second Calculator mode exists where "the seed" isn't the only way in. Marking this closed for now, not deferred — it'll resurface naturally if it becomes relevant.

**7. Decimal-column display — both your ideas are good and not in conflict.**
Trailing-zero auto-collapse (box shrinks when the value ends in 0) as the *default* view, plus a hover-to-reveal-full-precision as an *on-demand* view, aren't competing options — they're the collapsed and expanded states of the same control. My plan: build it exactly that way, collapsed by default, hover (or maybe a small expand-arrow, for touch/non-hover contexts later) reveals full precision including trailing zeros. No conflict to resolve, just confirming the mechanism.

**8. The long-horizon framing changes one thing about how I should work.**
"This is a decades-scale project, I want quality over speed, we can build until retirement" is a genuinely different brief than "get me something working fast." Practical effect: I'll lean noticeably harder toward flagging reversibility/cost-of-change on data-model decisions specifically (per point 2), even when it costs an extra exchange, because that's the category of decision that's cheap to get right early and expensive to fix at scale. I won't apply that same slowness to things that are genuinely cheap to redo later (UI polish, exact pixel values, non-committal settings) — those can move fast and loose, per your Wave 1 "session size" answer.

</span>

---

*RESOLVED sections are permanent. One real open item remains above (CHANGELOG confirmation) plus whatever new questions come out of the DISCUSSION section once you've read it — this is meant to be read as a conversation, not a form.*
