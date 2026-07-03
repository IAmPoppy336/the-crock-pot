# Building The Crock-Pot 🍲

*Plain-English guide. One path at a time. The bolded bits are the ones that matter.*

---

## The short version

This folder (`02_Build/`) is a complete, self-contained plugin project.
You don't install JUCE yourself — **the build downloads JUCE 8.0.13 automatically** the first time it runs.

There are two ways to get a plugin out of it. **Use Path A.** It builds Mac + Windows at the same time, on GitHub's computers, and runs the pluginval safety check automatically.

---

## Path A — Let GitHub build it (recommended)

1. Make a free account at github.com (if you don't have one).
2. Install **GitHub Desktop** (desktop.github.com) — it handles git for you.
3. In GitHub Desktop: **File → Add local repository** → choose this `02_Build` folder.
   (It will offer to "create a repository" — say yes, keep every default.)
4. Click **Publish repository**. Make it **Public** — public repos get GitHub's
   build machines for free; private ones have a monthly cap and macOS minutes
   count 10× against it (see docs.github.com → "About billing for GitHub Actions").
   The code contains nothing private — it's a gift recipe, and JUCE's free
   license is happy with open source.
5. On the repo's page on github.com, open the **Actions** tab. A run named
   **"The Crock-Pot"** starts by itself. Wait ~10–15 minutes for both green ticks.
6. Click the finished run → scroll to **Artifacts** → download:
   - `CrockPot-Windows` → contains `Crockpot_v0.0.1_<date>_win.zip`
   - `CrockPot-macOS` → contains `Crockpot_v0.0.1_<date>_mac.zip`

**If a job is red:** click it → click the failed step → copy the last ~30 lines
of the log back to Pi. That log is exactly what we need to fix it.

## Path B — Build locally on this Windows PC

Only needed if you want to skip GitHub. One-time installs:

1. **Visual Studio 2022 Community** (visualstudio.microsoft.com) — during
   install tick the workload **"Desktop development with C++"**.
2. **CMake** (cmake.org/download) — tick "Add to PATH" during install.
3. **Git** (git-scm.com) — defaults are fine. (CMake uses it to fetch JUCE.)

Then in a terminal (Start → type `cmd`), from inside the `02_Build` folder:

```
cmake -B Builds -DCMAKE_BUILD_TYPE=Release .
cmake --build Builds --config Release --parallel 4
```

First run downloads JUCE (~2 min) and builds (~5–10 min). Your plugin lands at:

```
Builds\CrockPot_artefacts\Release\VST3\The Crock-Pot.vst3
```

*(Mac equivalent, for reference: install Xcode Command Line Tools + CMake, same
two commands; artefacts land in `Builds/CrockPot_artefacts/Release/VST3` and `/AU`.)*

---

## Installing into Ableton Live 12

**Windows** — copy the whole `The Crock-Pot.vst3` folder into:
```
C:\Program Files\Common Files\VST3
```

**Mac** — from the mac zip, copy:
- `The Crock-Pot.vst3` → `/Library/Audio/Plug-Ins/VST3/`  (or the same path under `~/Library`)
- `The Crock-Pot.component` → `/Library/Audio/Plug-Ins/Components/`

Because the builds are **unsigned** (deliberate — gift path, decision D6):
- **Mac:** first open may be blocked → System Settings → Privacy & Security →
  scroll down → **"Open Anyway"**. (Sequoia asks for one extra confirm.)
- **Windows:** no blocking for plugin files; only .exe installers trigger
  SmartScreen, and we don't use one.

Then in Live 12: **Options → Preferences → Plug-Ins** → make sure **VST3** is on
(and **AU** on Mac) → click **Rescan**.

---

## The M0 test checklist (this is the milestone's exit criterion)

Run in Ableton Live 12. Tick every box; report any miss to Pi with what you saw.

- [ ] The plugin shows up in the browser under **Plug-Ins → VST3 → Poppys Kitchen → The Crock-Pot** — listed as an *audio effect*. (Note: Live happily lets audio effects sit on MIDI tracks too — they process the audio an instrument makes upstream. That's normal, not a bug. The real tell it registered correctly: it appears under Plug-Ins as an effect, not an instrument.)
- [ ] Drop it on an audio track playing a loop → **sound passes through unchanged** (no volume change, no glitches, no silence).
- [ ] Double-click the device's wrench icon → the window opens: **brown kitchen, a pot, three curls of steam, "The Crock-Pot"**, version stamp bottom-right.
- [ ] Drag the window corner → it **resizes smoothly**, nothing blurry or clipped.
- [ ] Group it into an **Audio Effect Rack** (Cmd/Ctrl+G) → it nests and keeps working.
- [ ] Save the Live set, close Live, reopen → plugin comes back, no error dialog.
- [ ] **Mac only:** the AU version also appears (browser → Plug-Ins → Audio Units); optionally run `auval -v aufx Ckpt Popy` in Terminal → ends with "PASS".
- [ ] Delete/undo works: Ctrl/Cmd+Z after deleting the device brings it back.

**All boxes ticked on both OSes = M0 is *verified-on-real-target* and we open M1** (audio passthrough → first Saturation block).

---

## Version bookkeeping

The version lives in one file: `VERSION` (currently `0.0.1`). We bump it every
build we hand to the brother-in-law, and the zip names carry the date, so no
build ever gets confused with another (roadmap M7 rule).
