# ArtHar Calculator — how this project uses it

**Repo:** `Harmelsson/arthar` (private, Rust). Separate project, actively developed by *other*
Claude sessions. **Read-only from here — never push to it.**
Attach it in a session with the `add_repo` tool (owner `Harmelsson`, repo `arthar`, access read),
clone to `/home/user/arthar`. Its `ARCHITECTURE.md`, `GLOSSARY.md` and `crates/calculator/src/`
are the authoritative reference.

**Last reviewed snapshot:** commit `8bf556f` (ArtHar v1.0), 2026-09-26.
When porting math, record the arthar commit it was ported from (in the C++ file header).

## What the calculator is (v1.0)
Pure math crate `crates/calculator` (no sound, no deps). Inputs → partials + scale notes.
- **stretch.rs** — `S = log2(seed)`, partial n = `f0 · n^S`. Seed 2 = plain harmonics.
- **loudness.rs** — rolloff: partial rank n gets `1/n^rolloff`; sticky per-partial loudness
  edits (factor on top of the rolloff); loudness 0 ⇒ partial excluded from the math.
- **manual.rs** — "Hand" mode: partials placed by hand in cents (id 1 = fundamental at 0 c).
- **dissonance.rs** — Sethares/Plomp-Levelt roughness (`D*=0.24, S1=0.0207, S2=18.96,
  B1=3.51, B2=5.75, C=5`), curve over unison→period at 0.5 c steps, dips refined, each dip has
  a **strength** 0–1 (fraction of curve swing), default min strength 2%.
- **consonance.rs** — cross-note method: largest note set where every pair shares ≥ N partials
  within a cents tolerance (default 5 c, min shared 2). Heavy search (can take seconds).
- **scale.rs** — `ScaleNote {degree, cents, home_cents, ratio, strength, kind}`; the scale is
  unison + found notes + **period** (= the seed's "octave", `1200·log2(seed)` cents).
  `NoteMoves` = sticky hand offsets of notes.
- **engine.rs** — `Calculator` holds all settings; `compute()` → partials, scale, curve.
- **export.rs** — .scl / .kbm / .tun.
- Design principles worth mirroring: stable addresses for every param (`ParamPath`), full f64
  precision, out-of-range values kept (dimmed, not hidden), edits stored relative to computed
  values.

## Integration decision (2026-09-26, reversible)
**Port the needed math to C++ (`shared/xen/`), using the Rust crate as the test oracle.**
- Why not link the Rust crate into the plugins (FFI/staticlib)? The arthar repo is private and
  changing under another session → CI would need cross-repo credentials, and every arthar change
  could break plugin builds. Plugins mostly need tiny pieces (stretch = one line, rolloff, the
  roughness formula), and the heavy searches must run off the audio thread anyway.
- Guardrail against drift: a check in `tools/` compares C++ results with the Rust calculator's
  output for a spread of settings (run in-session; the container needs `rustup update` to get
  rustc ≥ 1.95 first — found 1.94.1 on 2026-09-26).
- Port incrementally: stretch + rolloff first; dissonance curve when a plugin needs it;
  consonance search later.
