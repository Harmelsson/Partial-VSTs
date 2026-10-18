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

## JUCE 8 API gotchas
- `Font::getStringWidthFloat` no longer exists → `juce::GlyphArrangement::getStringWidth (font, text)`.
- Non-ASCII literals: `juce::String ("·")` reads bytes as Latin-1 → "Â·". Use `theme::u8 (...)`
  (String::fromUTF8). (2026-09-26)
- Don't name a Component method `hitTest(...)` with other args: hides `Component::hitTest(int,int)`
  (-Woverloaded-virtual).
## DSP
- One-pole smoothing never reaches its target exactly → anything testing `== 0` (sustain 0 ends a
  note) must snap when close. `smoothTowards` in SeedEngine does this now. (2026-09-26)
- **Release step 403 "Resource not accessible by integration"** (2026-09-26): runs #3 and #4 failed
  creating their Release; run #5 with the identical workflow succeeded (Release `build-5` exists).
  Cause unknown (both started within seconds of each other, right after the workflow first got
  `permissions: contents: write`). If it recurs: check repo Settings → Actions → General →
  Workflow permissions = "Read and write" (user must click it; agent can't).
- **Double-click events come from ANY mouse button** (2026-09-26): two quick middle-clicks fired
  mouseDoubleClick and added custom notes. Record the button in mouseDown and only act on left.
- **User preferences can flip quickly** (2026-09-26): the 12-keys mode was requested and withdrawn
  within one turn. Build big features in separable pieces so they can be removed cleanly.
- **Slicing code by "from marker A to marker B" deletes whatever sits between them** (2026-09-26):
  removing the old FX params also removed the chord/wander/scramble params → null-pointer crash in
  tests. After any bulk edit of the parameter layout, grep that every expected ID still exists.
- **Smoothed parameters need settle time in tests** (picks glides ~30 ms): render a warm-up block
  after setting params before measuring.

## New sessions start from `main` (2026-09-30)
Work pushed only to a `claude/*` branch is invisible to the next session (it checks out `main`, and
without CLAUDE.md on main it doesn't even know to look at other branches). Always also push to `main`.

## Parameters + state (2026-09-30)
- **APVTS saves PLAIN values in the state XML** (`<PARAM id="method" value="2.0"/>`: a choice is its
  index, a float its real value). So appending choices to a list keeps old projects correct. Only the
  host's *automation data* is normalised (index / (n-1)) and shifts when a list grows. Presets use plain values too.
- **Modulation links are not parameters**: they live in an `apvts.state` property ("modLinks"), which
  survives `replaceState`, and are mirrored to the audio thread via a fixed table of atomics (write
  `param = -1` first, `param = index` last so the audio thread never sees a half-written slot).
- **`AudioProcessorParameter::getParameterIndex()` is O(1)**: use it to index per-parameter arrays on
  the audio thread instead of a hash map.
- **Release that "cuts off"**: an exponential tail switched off at −60 dB is an audible step on headphones.
  End voices at ≤ −100 dB. Check with `SeedTests --release` (prints the tail level every 25 ms).
- **UI "is the note lit?" must not sample held keys only**: a note shorter than a UI refresh is never
  seen. Latch note-ons on the audio thread (ring buffer + counter) and read them from the UI.
- **Drag and drop between our own components**: the editor derives from `juce::DragAndDropContainer`;
  sources call `findParentDragContainerFor(this)->startDragging("mod:N", ...)`; targets derive from
  `juce::DragAndDropTarget` and find the editor via `dynamic_cast<ModDropHandler*>(container)`.
- **`SettableTooltipClient::setTooltip` is not virtual**; give composite controls their own `setHelp`.
- **`juce::Component::isShowing()` is false for an un-windowed editor** (headless snapshots) — use
  `isVisible()` for timers that should also run in tests/snapshots.
- **Linux container needs the JUCE dev packages** again in a fresh session:
  `apt-get install -y libasound2-dev libxrandr-dev libxinerama-dev libxcursor-dev libgl-dev libfontconfig1-dev libxcomposite-dev xvfb libxext-dev`.

## Buses, focus, scaling (2026-10-13)
- **JUCE process buffer layout**: input buses are laid out one after another from channel 0, output buses too, and
  both share those channels (in-place). With the main input disabled, an aux (sidechain) INPUT is channels 0-1 — the
  same ones the output goes to. Read inputs before writing outputs. Aux buses added with `withInput(..., false)` start disabled.
- **Discrete parameters need the full range to be modulated**: ±half range from 0 never crosses 0.5 of a switch.
  `getNumSteps() < 100000` tells discrete from continuous.
- **Never trust the old key-mode tests after removing a mode**: with the scale not yet calculated the engine falls back
  to a stretched 12-key layout, which made tests pass/fail by timing. Tests that want "normal keys" must set the
  EDO-12 method and wait for the calculator (`Rig` does this now).
- **Ambiguous constructor**: passing `*this` to a mixin base taking `Component&` while it also has a copy constructor
  is "ambiguous" — take a `Component*` instead.
- **Windows keyboard focus in hosts**: a click inside a JUCE plugin window leaves Win32 focus on the plugin's own HWND,
  so the host's typing-keyboard shortcuts stop working. Hand focus back with `SetFocus(GetAncestor(h, GA_ROOT))` after
  the click (not while a TextEditor / popup / modal is active). Cannot be tested on Linux.


## Feedback loops (Sane, 2026-10-17)
- **A loop with gain > 1 latches onto DC** unless something inside the loop blocks DC. The output DC blocker then
  "removes" the oscillation and you hear silence. Always keep a (compensated) DC guard inside every feedback loop.
- **Any high-pass inside a loop creates a weak low "phantom" mode** (where the HP's phase lead matches the loop delay).
  Don't fight it inside the loop (a stronger HP rings louder, and compensating its gain boosts the harmonics → runaway).
  Remove sub-bass OUTSIDE the loop (per-voice output high-pass that follows the pitch).
- **`I - c·H` (partial Householder) is NOT lossless** for 0 < c < 1: the mean direction is scaled by (1-2c) each pass and
  the whole FDN drains fast. Use orthogonal mixing (Givens rotations, or full H) and set decay only with the loop gains.
- **Compensate the phase delay of every in-loop filter at the mode frequency** (LP, DC guard) or modes go flat by
  several cents; a zero-phase-at-centre band-pass needs none. With that + 3rd-order Lagrange, pitch is exact to 0.1 c.
- **Measure glides early**: an energy-driven pitch glide is mostly over after ~20 ms; analysis windows must start at ~5 ms.
- **Analyse rendered WAVs numerically** (peaks, sub-band energy, decay per window) — it caught 3 problems the pass/fail
  tests didn't (quiet cores, rumble, fast membrane decay). `SaneTests --wav dir` + numpy in the container.

## CI timing (2026-10-17)
- With Seed + Sane (both LTO, VST3 + Standalone, two test apps) the Windows Build step takes ~26 min. Pushing the same
  commit to the branch AND main starts two full runs. Plan check-ins ~30 min after a push. Possible speed-up: drop LTO
  for the test apps, or only build Releases from main.
- **A dry/wet mix INSIDE a feedback loop is a filter, not a mix**: for time-varying effects (freq shift, chorus) the dry and
  wet copies interfere on every pass and drain the loop (thin tail at 50 %). In loops: fully wet + an intensity knob.
- **Calibrate the loop-internal level** before judging nonlinear inserts: at 0.05 a tanh saturator is a straight wire.
  Measure it (probe) per core and normalise to ~1.0.
- **Separate "timbre" controls from "loudness"**: strike position and burst length must keep energy constant, otherwise
  they fight velocity. Measure loudness as energy over a window, never as peak, when the number of modes differs.
- **Order matters for "after" effects**: anything placed after a distortion (here a high-pass) reshapes it. Put user
  after-effects last in the voice chain.
- **Voice-end detection with long loops**: silence between repeats is not the end — wait longer than one loop pass.
