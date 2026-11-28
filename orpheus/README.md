# ORPHEUS

SIREN (the synth) with its ten effects, played by ORPHEUS: you draw four chord lines and it plays them,
sliding from chord to chord. A **VST3 instrument** and a **standalone app** for Windows.

## Install

1. Open the repo's **Actions** tab, then **ORPHEUS (Windows)**, then the newest green run.
2. Download **ORPHEUS-x.y.z-Windows** from the run's *Artifacts* box and unzip it.
3. App: run `ORPHEUS.exe`. In the bar at the top, *Options → Audio/MIDI Settings* picks the sound card and a MIDI keyboard.
4. VST3: copy the `ORPHEUS.vst3` folder to `C:\Program Files\Common Files\VST3`, then rescan in your DAW.
   ORPHEUS is an instrument: put it on an instrument / MIDI track.

The window needs the Microsoft Edge WebView2 Runtime, which is part of Windows 10 and 11.

## Using it

- **In a DAW** ORPHEUS follows the DAW: press play in the DAW and the song plays in time with it (looped, or once with
  LOOP off). With the DAW stopped, ORPHEUS's own PLAY button plays the song at its own tempo.
- **In the app** the PLAY button plays the song.
- **MIDI** (a keyboard, or notes in a DAW clip) plays SIREN as well, as chords, on a voice of its own, so you can play
  along with the song. Pitch bend is ±2 semitones; the mod wheel adds vibrato.
- Every SIREN setting is a DAW parameter (automation works); the song and the sound are saved with the project.
- Songs can be up to 999 bars: the BARS control in ORPHEUS's header, or just draw past the end.

## How it is built

The window is the same page as the browser preview (`ui/src`, built into `WebUI/index.html` by `tools/make_ui.py`).
In the plugin the page only shows and edits: `WebUI/orpheus-bridge.js` sends every setting, the song and the
PLAY/STOP buttons to the C++ engine, which makes the sound:

| File | What |
|---|---|
| `Source/DSP/SirenSynth.*` | SIREN's voice: wavetable oscillators, unison, sub, noise, OCTAVE, drive, the seven filters, envelopes, LFO, character |
| `Source/DSP/Effects.*` | DRIVE, CRUSH, RING, CHORUS, FLANGER, PHASER, DOUBLER, DELAY, REVERB, SHIMMER, WIDTH, SPACE, compressor, limiter |
| `Source/DSP/Shimmer.h` | the shimmer reverb (the same code as the preview's `fx-worklet.js`) |
| `Source/DSP/ReverbIR.h` | the reverb's impulses, grown like the preview's |
| `Source/Seq/Song.h` | the song and how its lines slide |
| `Source/PluginProcessor.*` | the instrument: the player, the DAW's transport, MIDI, the saved state |
| `Source/PluginEditor.*` | the window (WebView2) |

The C++ engine is a copy of the preview's Web Audio engine, made to sound the same: the Web Audio filters' own
formulas (a lowpass's Q is in dB there), the same envelopes, the same effect settings. `OrpheusTest --cases` renders
the same notes as the preview's capture script so the two can be compared; single voices match to 0.1 dB and the
effects to about 1 dB (the reverb's random impulse differs a little every time, in the preview too).

After changing the UI:  `python3 tools/make_ui.py`. After adding a SIREN setting:  `python3 tools/gen_params.py`.
After changing the default song:  `python3 tools/gen_song.py`.

## Building

- Windows: `build_windows.bat` (Visual Studio 2022 with C++, CMake, nuget). JUCE 8.0.10 is downloaded by CMake.
- GitHub builds it on every push to the `orpheus` branch (`.github/workflows/orpheus.yml`) and runs the engine test
  first: the default song, every effect, random settings, MIDI, the saved state and all 83 presets
  (`Tests/presets.json`), failing on silence, NaN, clipping or runaway levels.
