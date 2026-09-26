# Status

_Last updated: 2026-09-26 (session 1)_

## What exists
- Workflow + docs only: `CLAUDE.md` (session protocol), `docs/` (STATUS, LOG, LESSONS, IDEAS).
- **No code yet.** No JUCE project, no CI, no plugins.
- ArtHar calculator (Rust, `Harmelsson/arthar`, v1.0) reviewed → `docs/ARTHAR_REFERENCE.md`.
  Separate project; math is *copied* to C++ (`shared/xen/`), never linked.

## Decided
- First plugin: **Seed** (user loves the name). Smooth, friendly synth UI (gentler scrubbing).
- Math from ArtHar is copied to C++, not linked.

## Proposed plan
1. **Task 0 — Pipeline ("v0.0")**: JUCE CMake scaffold + shared safety stage + GitHub Actions
   that builds Windows VST3 → user downloads a zip, drops it in
   `C:\Program Files\Common Files\VST3`, FL Studio finds it. Plus `docs/TESTING.md` (install guide).
   Proven with the first real plugin, not an empty "hello" plugin.
2. **Task 1 — first plugin**: recommended **Seed** (I1 in IDEAS.md), alternative **Stretch
   Resonator** (F1).
3. Then alternate: one weird experiment ↔ one building-block tool.

## Open questions for the user
- Branch/merge policy: each session runs on its own `claude/*` branch. Merge to `main` after each
  session (via PR), or allow the agent to merge itself? Unmerged branches risk a future session
  not seeing the latest docs.
- Can you download build artifacts from GitHub (logged-in, Actions tab)? Alternative: GitHub
  Releases page.

## What to test right now
Nothing yet.
