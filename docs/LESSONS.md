# Lessons & Gotchas

Things that failed, surprised us, or cost time. Read before build/DSP/FL work.
Format: **topic** — what happened → what to do instead. (date)

## Environment
- **Agent container** (2026-09-26): Linux, cmake 3.28, g++ 13, clang, ninja, 4 cores available;
  github.com reachable (JUCE clonable). → Compile + headless tests locally; Windows builds via CI.

## Known-in-advance (not yet hit, but expected)
- FL Studio caches its plugin list: after installing a *new* plugin the user must run
  *Options → Manage plugins → Find installed plugins*. Replacing an existing `.vst3` with a
  newer build usually just needs FL restarted (or the plugin re-added).
- Each plugin needs a unique 4-char `PLUGIN_CODE` (and a shared `PLUGIN_MANUFACTURER_CODE`);
  collisions make hosts confuse plugins.
- Windows locks a loaded `.vst3`: FL must be closed before overwriting it.
- **Rust version** (2026-09-26): container ships rustc 1.94.1; arthar requires ≥ 1.95 →
  `rustup update` before running the Rust calculator as an oracle.
- **arthar repo moves under us** (2026-09-26): another session edits it concurrently. Always
  note the commit hash you read/ported from; never push there.
