# Status

_Last updated: 2026-09-26 (session 1)_

## IN PROGRESS — Seed Playground (v0.3.x)
User is away for a few hours; asked for a big experimental "chaotic mess" to curate on return.
Plan + phase checklist: `docs/PLAYGROUND.md`. Each phase is committed separately; if a session
ends mid-way, continue at the first unchecked phase there.

## What exists
- **Seed v0.2.0** (`plugins/Seed/`, see its README): ArtHar-style redesign; Calculator bar on top
  (Fundamental, Seed, Partials, Rolloff, Min dip strength) with live dissonance curve, partials
  plot and scale table; full ADSR (draggable envelope picture) + velocity sensitivity.
  35 headless checks pass on Linux (`tools/SeedTests.cpp`), incl. the C++ dissonance copy
  reproducing ArtHar's pinned notes (seed 2: 315.6 386.3 498.0 582.5 702.0 813.7 884.4 968.8).
- **Shared kit for all future synths** (`shared/`): `xen/` Stretch, Dissonance (copy of ArtHar's
  method), CalculatorWorker (background thread); `ui/` Theme (ArtHar two-colour), Controls
  (per-digit ScrubBox, SegmentedToggle), SquareLattice (square-dot plots + afterglow),
  CalculatorViews (curve / partials / scale table), EnvelopeView; `dsp/` SafetyStage.
- Pipeline: GitHub Actions builds Windows VST3 + standalone on every non-docs push, runs tests,
  publishes a GitHub **Release** `Build N` (front page → Releases). Run #2 = first green build;
  Release `build-5` = v0.1.0; **Release `build-6` = Seed v0.2.0** (green, 08:00 UTC):
  https://github.com/Harmelsson/Partial-VSTs/releases/tag/build-6

## User should test (Seed v0.2.0) — install guide: docs/TESTING.md
1. Look & feel vs ArtHar. Scrub digits of Seed while a note is held: curve, table, partials and
   the sound should all move together.
2. Velocity: soft vs hard notes. Envelope: drag the three hollow corners; try Sustain 0 (plucks).
3. Fundamental changes the curve (roughness depends on Hz); with Keys = Seed it also moves key 60.
4. Min dip strength: more/fewer notes in the table. Partials 64: curve still responsive?
5. Scrub sensitivity: 18 px per step — too slow/fast?

## Next
- **Seed v0.2**: user wants to TRY SEVERAL of the ideas in `docs/SEED_NOTE_LIFE.md` ("variety,
  no harm"). Plan: a "note life" mode selector, starting with Magnet keyboard + Life knob, then
  Valley riders / Mitosis / Orphans / Degree keyboard as extra modes. Needs C++ copy of the
  dissonance curve first.
- Possibly: per-digit scrubbing for the seed value (ArtHar-style) if the knob is too coarse.

## Open questions for the user
- Merge policy: each session runs on its own `claude/*` branch. Merge to `main` after each session,
  or let the agent merge? (Unmerged work may be invisible to the next session.)
