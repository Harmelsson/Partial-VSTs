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

## Build / test
- **JUCE strict warnings** flag every float `==`; use `juce::exactlyEqual` / `<=` instead. (2026-09-26)
- **Testing the processor headlessly:** don't link the plugin target (JUCE modules get compiled
  twice). Compile the plugin's .cpp files straight into a `juce_add_console_app` instead — works
  because our sources don't use `JucePlugin_*` macros. Keep it that way. (2026-09-26)
- **UI check without a screen:** `xvfb-run -a SeedTests --snapshot x.png` (needs
  JUCE_MODAL_LOOPS_PERMITTED=1 in the test target to let animations settle). (2026-09-26)
- Linux deps for JUCE (apt): libasound2-dev libxrandr-dev libxinerama-dev libxcursor-dev libgl-dev
  libfontconfig1-dev libxcomposite-dev (+ xvfb). Configure with
  `-DFETCHCONTENT_BASE_DIR=/home/user/.cache/pvst-deps` so JUCE isn't re-cloned per build dir.
- Windows builds link the C++ runtime statically (`CMAKE_MSVC_RUNTIME_LIBRARY`) so the user never
  needs a VC++ redistributable.
- **User couldn't find the build** (2026-09-26): "Actions → Artifacts" is too hidden. Builds are now
  published as GitHub Releases (front page, right side). Windows CI timing: configure ~4.5 min
  (JUCE clone + juceaide), build with LTO ~10+ min.
- **Agent can't fetch CI artifacts** (2026-09-26): the container's egress proxy blocks
  `*.blob.core.windows.net`, so artifacts can't be downloaded here and re-sent to the user.
  Give the user the run URL (Artifacts at the bottom) or rely on the Release.
- **First Windows CI build: green on run #2** (~14 min: configure 4.5, build ~9). Docs-only pushes
  are now skipped (`paths-ignore`) and superseded runs cancelled (`concurrency`).
- **Cross-compiling Windows from Linux** was considered and rejected (unofficial toolchain, can't
  test the result here). CI on a real Windows runner stays the source of user builds. Possible
  speed-up later: Ninja + sccache in CI.
