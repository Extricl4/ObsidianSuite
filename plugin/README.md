# Obsidian Suite: the plugin

The real Obsidian Suite as a **VST3** and a **standalone app** (Windows, ASIO), built with JUCE.
The interface is the same page as the browser demo (`ui/obsidian-demo.html`). Inside the plugin it is
connected to the audio engine by `WebUI/obsidian-bridge.js`.

## Get it

* **Easiest:** open the repo's **Actions** tab → *Plugin (Windows)* → the newest green run → download
  **ObsidianSuite-…-Windows** (installer `.exe` + zip). Every push to `plugin/` or `ui/` builds a new one.
  Tags like `v0.3.0` also publish a GitHub release.
* **Build it yourself:** install Visual Studio 2022 ("Desktop development with C++"), CMake and Git, then run
  `plugin\build_windows.bat`. The first build downloads JUCE, NAM and the other libraries (about 10 minutes).

The installer puts the VST3 in `C:\Program Files\Common Files\VST3` and the app in the Start menu.

### First start (standalone)
Settings → **Device**: choose **ASIO** and your interface (e.g. Focusrite USB ASIO), buffer **128** (or 256).
If an ASIO driver is installed the app picks it by itself on the first start. Input is never muted.

## What's inside

| Part | Where |
|---|---|
| Signal chain: input → gate → transpose → synth (pre) → pedals → amp → cab → EQ → FX → special FX → doubler → width → output | `Source/DSP/Engine.cpp` |
| Amps: built-in *Obsidian Core* (no files needed), *NAM Loader* (up to 4 `.nam` captures) | `Source/DSP/AmpSection.h`, `NamEngine.*` |
| Cab: two mics (model, position, distance, level, pan, solo/mute/phase), IR loading, built-in cabs | `Source/DSP/CabSection.h` |
| 30 pedals / effects (drive, fuzz, boost, comp, wah, whammy, octave, harmonizer, sub, delay, reverb, shimmer, swell, freeze, tape stop, looper, glitch, chorus, flanger, phaser, tremolo, vibrato/rotary, ring mod, bitcrusher, filters, limiter) | `Source/DSP/Modules/*.cpp` |
| Transpose: **Live** (instant) and **HQ** (studio quality, latency reported to the DAW) | `Source/DSP/Sections.h`, `PitchShifter.h` |
| Guitar synth: tracking, 2 oscillators + unison, ladder filter, arpeggiator | `Source/DSP/GuitarSynth.h` |
| Parameters (one list: DAW automation, presets and UI all use it) | `Source/Core/Params.cpp` |
| Presets (factory = `Presets/*.json`, yours = `Documents\Obsidian Suite\Presets\*.obsidian`) | `Source/Core/PresetManager.*` |
| Amp / cab / pedal choices, loading files in the background | `Source/Core/Rig.*` |
| Your settings (theme, folders, MIDI, audio options) | `Source/Core/AppSettings.h` → `%APPDATA%\Obsidian Suite\settings.xml` |
| MIDI learn / CC mapping / program change | `Source/Core/MidiMapper.*` |
| UI ↔ engine bridge (all commands in one table) | `Source/Editor/UiBridge.cpp`, `WebUI/obsidian-bridge.js` |
| Standalone app (audio device settings live in the UI) | `Source/Standalone/StandaloneApp.cpp` |

**Never-crash rules the code follows:** nothing allocates or locks on the audio thread; files load on a
background thread and are swapped in without a click; every output is checked for NaN/Inf and runaway
level; broken or missing files show a message and the old sound keeps playing; old presets and DAW
projects keep loading when parameters are added or removed.

## Your files: `Documents\Obsidian Suite`

```
Presets\    your presets (.obsidian, plain JSON; share them as files)
Captures\   .nam files (amps and pedals)
IRs\        cab impulse responses (.wav)
Amps\       one .json per amp you want in the ADD AMP list
Cabs\       one .json per cab you want in the ADD CAB list
```

An amp (`Amps\styx.json`). Paths are relative to `Captures\` (or absolute):

```json
{ "key": "styx", "name": "Styx 5150", "role": "high gain",
  "description": "Peavey 5150 lead channel, captured.",
  "captures": [ "5150-clean.nam", "5150-crunch.nam", "5150-lead.nam" ] }
```

Each capture is a channel (CH 1–4 on the AMP page, also automatable as *Amp Channel*).
The GAIN/BASS/MIDDLE/TREBLE/PRESENCE/DEPTH/MASTER knobs work on every amp.

A cab (`Cabs\v30.json`), paths relative to `IRs\`:

```json
{ "key": "v30", "name": "4x12 V30", "size": "4x12", "irA": "v30-sm57.wav", "irB": "v30-r121.wav" }
```

## Changing things later

**Add a knob or switch**
1. `Source/Core/Params.cpp`: add one line, e.g. `p.f ("amp.sag", "Amp Sag", 0.f, 10.f, 3.f);`
2. Use it in the DSP: add a pointer next to the others in `Engine.h` (`*ampSag`), fetch it in `Engine::attach`, and read it in `Engine.cpp` with `v (p.ampSag)`.
3. UI: put a knob in the page, then one line in `setupBindings()` in `WebUI/obsidian-bridge.js`:
   `knob('amp.sag', someKnobElement, v => v.toFixed(1));`

Presets, DAW automation, MIDI learn (right-click any knob) and old presets all follow automatically.

**Add a pedal or effect**: write a class in `Source/DSP/Modules/` (copy a small one like `Pulse Tremolo` in
`ModModules.cpp`), then add one `t.push_back ({ key, name, category, racks, description, { knobs… }, factory })`
line in that file's registry. It appears in the ADD PEDAL/EFFECT list with its knobs. The self-test checks it.

**Add a UI command**: one entry in `UiBridge::buildCommands()` and call it from JS with `call('name', …)`.

**Remove something**: delete its line. Presets that still mention it simply ignore it.

**Change the UI**: edit `ui/src/*` and run `python ui/src/build.py` (rebuilds `ui/obsidian-demo.html`).
Live editing without rebuilding the plugin: set *uiDevFolder* in `settings.xml` (or the environment
variable `OBSIDIAN_UI_DEV`) to your `ui` folder and reopen the plugin window. Files found there win over the
built-in copies (copy `WebUI/obsidian-bridge.js` in too if you're editing the bridge).

**Factory presets**: change the sounds in `writeFactoryPresets()` (`Source/Tests/SelfTest.cpp`) and run
`"Obsidian Suite.exe" --write-factory-presets plugin\Presets`, or save a preset in the plugin and copy
the file into `Presets\` (rename `.obsidian` to `.json`).

**Version**: change `VERSION`.

## Tests

* `"Obsidian Suite.exe" --selftest report.txt` renders every pedal in every slot with random settings,
  every sample rate and buffer size, checks transpose pitch accuracy, the synth, presets, DAW state and
  CPU. Add `--nam file.nam` to test a capture. The GitHub build runs it on every push.
* `python plugin/tools/ui-test/run.py` runs the real UI in headless Chrome against a fake engine and clicks
  through every page (screenshots in `tools/ui-test/out`). Refresh its data with
  `"Obsidian Suite.exe" --dump-ui-state plugin/tools/ui-test/state.json`.

## Not done yet

* **STUDIO** (the mini recorder) is shown only in the standalone app, and recording there is still the
  demo. The engine side (recording the DI and re-amping takes) is the next step.
* Amp and pedal **graphics**: the AMP page shows a clean generic panel until the amps from the visuals
  chat are added. Their knobs map to the same parameters.

Licences of the libraries used: [LICENSES.md](LICENSES.md).
