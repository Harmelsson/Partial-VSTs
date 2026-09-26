# How to install and test the plugins (Windows + FL Studio)

## 1. Download a build
1. Open the repo's front page on GitHub. On the right side there is a **Releases** box.
2. Click the newest release (**Build N**), then under **Assets** click `PartialVSTs-build-N.zip`.
3. Unzip it. Inside: `VST3/` (the plugins), `Standalone/` (the same plugins as normal apps).

A release only appears once a build has fully succeeded (compiled + all tests passed).
To see a build that's still running: the **Actions** tab at the top of the repo page
(yellow dot = running, green ✓ = done, red ✗ = failed — the agent fixes red ones).

## 2. Install into FL Studio
1. **Close FL Studio** (Windows locks plugin files that are in use).
2. Copy everything inside `VST3/` (e.g. the `Seed.vst3` folder) to
   `C:\Program Files\Common Files\VST3\` (Windows asks for admin permission — allow it).
   Replacing an older version: overwrite it.
3. Start FL Studio. First time only: **Options → Manage plugins → Find installed plugins**.
   The plugins appear under the manufacturer **Partial**.
4. Add it: Channel rack → **+** → search "Seed".

Check the tiny grey text in the bottom-right of the plugin window: `v0.1.0 · build N · abc1234`.
**N must match the build number you downloaded** — otherwise FL is still using an old copy.

## 3. Quick try without FL (optional)
`Standalone/Seed.exe` runs Seed as its own app (pick your audio device in its Options menu).
The **Drone** switch makes sound without any keyboard.

## 4. Reporting back
Anything is useful, in any form: what you did → what you heard/saw → what you expected or wished.
Screen recordings with sound are gold. Mention the build number.

## Safety
Every plugin has a built-in limiter at −1 dBFS that can't be switched off, but **start with your
speaker/headphone volume low** on a new build anyway.
