# Obsidian Amp Suite: the plugin

The real Obsidian Amp Suite as a **VST3** and a **standalone app** (Windows, ASIO), built with JUCE.
The interface is the same page as the browser demo (`ui/obsidian-demo.html`). Inside the plugin it is
connected to the audio engine by `WebUI/obsidian-bridge.js`.

## Get it

* **Easiest:** open the repo's **Actions** tab → *Plugin (Windows)* → the newest green run → download
  **ObsidianAmpSuite-…-Windows** (installer `.exe` + zip). Every push to `plugin/` or `ui/` builds a new one.
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
| Signal chain: input → gate → transpose → synth (pre) → pedals → [HYDRA before amp] → amp → cab → [HYDRA after amp] → EQ → FX → special FX → doubler → width → output | `Source/DSP/Engine.cpp` |
| Amps: 9 in-house amps in the **OBSIDIAN AMPS** folder (modelled circuit by circuit, see below), *NAM Loader* (up to 4 `.nam` captures) | `Source/DSP/AmpModels.h`, `AmpSection.h`, `NamEngine.*` |
| Cab: two mics (model, position, distance, level, pan, solo/mute/phase), IR loading, built-in cabs | `Source/DSP/CabSection.h` |
| 29 pedals / effects (drive, fuzz, boost, comp, wah, whammy, octave, sub, delay, reverb, shimmer, swell, freeze, tape stop, looper, glitch, chorus, flanger, phaser, tremolo, vibrato/rotary, ring mod, bitcrusher, filters, limiter) | `Source/DSP/Modules/*.cpp` |
| 9 pack pedals with their own captures / IRs (Chiron, Emperor, Graven, Maw, XXXIII, Helios, Aeon; Ossuary, Halcyon) plus Crypt's six RV-6 modes | `Source/DSP/Modules/PackModules.cpp`, `IrCore.h` |
| 10 celestial pedals: lush reverbs (Elysium hall, Empyrean two-voice shimmer, Nimbus cloud, Vesper choir, Halo reverse/bloom), delays (Astral shimmer delay, Mnemosyne three-head tape echo + spring, Gemini dual/reverse), Lyra 12-string, Icarus octave fuzz | `Source/DSP/Modules/CelestialModules.cpp` |
| Transpose: **Live** (instant) and **HQ** (studio quality, latency reported to the DAW) | `Source/DSP/Sections.h`, `PitchShifter.h` |
| Synth, played by the guitar or MIDI (see below) | `Source/DSP/GuitarSynth.h`, parameters in `SynthDefs.h`, page in `ui/src/synth.js` |
| HYDRA harmonizer, six voices (see below) | `Source/DSP/Harmonizer.h`, parameters and modes in `HarmonyDefs.h`, page in `ui/src/harmony.js` |
| Themes: colours, fonts, bands and moving details | `ui/src/themes.js`, fonts in `ui/fonts` (OFL) |
| Parameters (one list: DAW automation, presets and UI all use it) | `Source/Core/Params.cpp` |
| Presets (yours: `Documents\Obsidian Amp Suite\Presets\*.obsidian`; factory presets: `Presets/Factory` and `Presets/Artists/<artist>`, 360 of them) | `Source/Core/PresetManager.*` |
| Amp / cab / pedal choices, loading files in the background | `Source/Core/Rig.*` |
| Your settings (theme, folders, MIDI, audio options) | `Source/Core/AppSettings.h` → `%APPDATA%\Obsidian Amp Suite\settings.xml` |
| MIDI learn / CC mapping / program change | `Source/Core/MidiMapper.*` |
| UI ↔ engine bridge (all commands in one table) | `Source/Editor/UiBridge.cpp`, `WebUI/obsidian-bridge.js` |
| Standalone app (audio device settings live in the UI) | `Source/Standalone/StandaloneApp.cpp` |

**Never-crash rules the code follows:** nothing allocates or locks on the audio thread; files load on a
background thread and are swapped in without a click; every output is checked for NaN/Inf and runaway
level; broken or missing files show a message and the old sound keeps playing; old presets and DAW
projects keep loading when parameters are added or removed.

## The in-house amps

No files needed: each one is modelled stage by stage (the real tone stack's component values, cascaded
triode stages with their cathode and coupling filters, cathode follower, power amp with sag,
presence/depth in the feedback loop, output transformer), oversampled 4x. All are level-matched.
The amp chooser keeps them in the **OBSIDIAN AMPS** folder.

| Amp | Based on | Role |
|---|---|---|
| GLASS 65 | Fender '65 Twin Reverb | clean |
| TOP 30 | Vox AC30 Top Boost | clean, crunch |
| PLEXI 68 | Marshall 1959 Super Lead (jumped) | crunch |
| BRIT 800 | Marshall JCM800 2203 | crunch, high gain |
| SLO 100 | Soldano SLO-100 (overdrive channel) | high gain, lead |
| STYX 51 | Peavey 5150 / EVH 5150 (lead channel) | high gain |
| RECTO | Mesa/Boogie Dual Rectifier (modern) | high gain |
| VH-4 | Diezel VH4 (channel 3) | high gain |
| OBSIDIAN CORE | Obsidian original | modern high gain |

## The capture amps (the amp pack)

The 30 finished amps from the visuals chat (`visuals/finished/`) and the plugin's own Nemean and Phlegethon are in the plugin, in the **CAPTURE AMPS** folder
of the amp chooser, each with its real face: the layer renders (face on / off, a pointer per knob, a sprite per lit
LED), so knobs turn, buttons and LEDs work, the lamp is the power switch, and Moirai / Typhon change colour with the
channel.

* **Sound.** Each amp is many NAM captures of the real amp (433 in all). `Content/Amps/<key>.json` ("map") says which
  capture plays for every knob and button setting. The engine (`Source/DSP/CaptureAmp.h`) compiles that map and
  plays it: a knob that steps through captures (gain steps, Umbra's Middle, the grid of Chrysaor and Kratos) blends
  the two nearest captures; Astraea plays the two captures closest to your Volume / Treble / Bass / Tone Cut;
  Eos's SOLO knob picks the Comfortably Numb rigs. Every capture of the amp is loaded when you choose it, so
  turning knobs never waits for a file.
* **The other knobs.** Gain, Bass, Middle, Treble, Presence, Depth / Resonance and Master are the plugin's AMP
  parameters (automation, MIDI learn, presets). Where a knob picks captures, it does that; otherwise Bass / Middle /
  Treble / Presence / Depth are a tone stack after the capture and Master is the level. Extra knobs: Volume /
  Output / Level (trims), Tight, Air, Body, Reverb, Speed + Intensity (Melinoe's tremolo), Persephone's chorus
  (CHORUS button + Speed / Depth), Kratos's CRUNCH (more drive) and BRIGHT. Amps whose channels keep their own
  knobs (Moirai, Typhon, Umbra, Lamia...) swap them with the channel, also when a DAW automates the channel.
* **Cab in the capture.** Captures that already contain a cab (marked "rig") bypass the CAB section while they play.
  An amp whose `kind` starts with "Full rig" counts every setting as a rig (Belial, Ares, Lyssa, Melinoe and Nihil
  weren't marked and went through two cabs, which is why their presets sounded muffled; fixed 2026-11-22).
* **The plugin's own amps.** Two amps aren't from the visuals chat: **Nemean** (Pantera, the "Pantera Full Tone"
  captures by hellyeahdime: COWBOYS / VULGAR / DRIVEN / TRENDKILL pick the album, and the glow behind the claw
  slashes, the light strip, the power lamp and the LED take that album's colour) and **Phlegethon** (Gojira, the
  "Black Go" capture of the Archetype: Gojira X plugin by cyclone: a slab of cooling magma). Their wiring is
  `Content/Amps/nemean.json` / `phlegethon.json` (written by hand). Their looks are Blender renders made here:
  `tools/faces/amps/make_amps.py` (art + specs), `tools/faces/amps/render.sh` (a copy of the visuals renderer,
  so nothing is written into `visuals/`), then `tools/faces/build_own_amps.py` makes their faces.
* **Level.** When an amp loads, its default capture plays a fixed test riff and the amp is trimmed so every amp
  plays about as loud as the in-house amps (the self-test measures them: −32.7 to −25.5 dB, in-house −29.5).
* **Files.** `Content/Amps/*.json` (the wiring, from `visuals/finished/export_amps.py`), `Content/Captures/Amps/`
  (the captures), `Content/Faces/` (positions + images, from `tools/faces/build_faces.py`). The installer's pack
  option installs them in `Documents\Obsidian Amp Suite` (Amps and Faces are always updated); the release zip has
  them under `Content`.

## Pedal faces

The 31 pedal designs from the visuals chat (`visuals/pedals/*.json` + the artwork in `visuals/art`) show on the
pedal board as their real faces: enclosure, artwork, knobs at their positions, toggles, LED and footswitch. The
LED and the glowing parts of the artwork light up when the pedal is on, and the drawn footswitch switches it.
`tools/faces/build_pedals.py` draws them (the 3D renders need Blender) into `WebUI/pedals/*.webp` and
`WebUI/obsidian-pedals.js`; they are compiled into the plugin.

## The synth

Played by the guitar (pitch tracking, bends come through) or by MIDI notes from a keyboard or the DAW.
On the SYNTH page, **AUDITION** and the computer keys A–K play it too.

* **Play**: threshold, sensitivity, glide, dynamics, LEGATO, **HOLD** (the last note drones on),
  **CHORD** (one note plays octaves, power chord, sus, or a triad / 7th / 9th in your key and scale)
* **Sound**: oscillators A and B (saw, pulse, triangle, sine, super) with shape, 1–7 voice unison,
  detune and stereo width; B can hard-sync to A; sub oscillator; coloured noise. Oscillators and filter
  run at twice the sample rate.
* **Filter**: ladder 24 / 12 dB, lowpass, bandpass, highpass, notch, **vowel** (A-E-I-O-U formants),
  drive, envelope amount, key tracking. Drag the curve to set cutoff and resonance.
* **Envelopes** (amp and filter, one graph), **LFO** (6 shapes, tempo sync, fade in; vibrato, wah, shape,
  tremolo or pan)
* **Arpeggiator**: 16 steps, each with on, accent, tie, octave, ratchet (2–4 repeats) and chance; up,
  down, up-down, random or **steps** (each step picks a scale note); swing; tempo from the DAW or the app.
  The steps are saved with the preset.
* **Output**: level, mix, space (stereo echoes), **character** (clean, analog, 8-bit, VHS), before or after the amp
* **DICE** rolls a whole new sound (keeps mix, level, key and your playing settings), with undo.
* **Presets**: 75 sounds in folders (arcade, cartoon, leads, bass, pads, arps, FX). The **HORROR** folder
  (blood red) has 12 that each bring their own atmosphere: loading one also sets AETHER and turns it on.

### AETHER (the SYNTH page's second page)

The switch above the display flips between **SIREN** (the synth) and **AETHER**: noise, ambience and
atmospheres that play by themselves around your playing, no notes needed. It has its own POWER
(independent of SIREN's), and its dot on the switch lights while it plays. Engine: `Source/DSP/Atmos.h`
(settings `AtmosDefs.h`, `atm.*`), added after the cab and synth, before the EQ and FX.

* **BED**: hiss, wind (gusts, a howl), rain (drops on a hiss), surf (waves left and right), rumble (deep
  earth, a sub), static (an old radio, crackle, a tuning whistle), breath (slow breathing, rasping when DEEP).
  The knobs are named for what they do with each bed (WIND: PITCH, GUSTS, HOWL).
* **DRONE**: organ, strings, choir (drifting vowels), glass (bell FM), abyss (a growling sub), descent and
  ascent (Shepard tones that fall or rise forever); chords from one note to a dread cluster; octave;
  in SIREN's key, or **FOLLOW** moves it to the note you play.
* **TEXTURE**: vinyl, fire, drips, metal (struck and scraped plates), shimmer (glass in your key), whispers,
  heart (STUMBLE makes it skip), knocks, moans (voices in pain), flesh (wet, cracking, tearing).
* **VOID**: SIZE (a room to an endless void) and SPACE, drawn live. In the strip: EVOLVE (everything drifts),
  FOLLOW (duck under your playing or swell with it), **FREEZE** (holds the space forever), WIDTH, GRIT, TONE, LEVEL.
* **MOOD** (calm, dream, eerie, dread, horror) decides what the three dice, **MUTATE** (nudge everything)
  and **SUMMON** (a whole new world, with a name) reach for. **SCENES**: 29 ready-made atmospheres
  (nature, dream, dark, horror), levelled so they sit under the guitar.

## HYDRA, the harmonizer (HARMONY page)

Up to six voices. Each one is **SMART** (a step in the key and mode, so the semitones follow the note you
play: a 3rd up is +4 on one note and +3 on the next) or **FIXED** (always the same number of semitones),
with its own level, pan, detune (cents) and delay (up to 120 ms). 45 modes (major and minor, the church
modes, pentatonics and blues, harmonic and melodic minor families, Phrygian dominant, Hungarian minor,
diminished, whole tone, Hirajoshi, bebop and more). Notes outside the mode either take the interval of
the nearest scale note (FOLLOW) or keep the last harmony (HOLD). HUMANIZE adds a little drift in pitch,
timing and level; SPREAD widens or narrows the pans; MIX is dry and harmony at full in the middle.
PLACE: after the amp (harmonies of your amp sound, stereo) or before it (the voices go through the amp).

* **LEAD** (single notes, the main mode). Its own pitch tracker listens to the clean guitar (NSDF, the
  window grows with the period: a high note is known in a couple of periods). Each voice is a
  pitch-synchronous shifter: a read head moving through the signal at the pitch ratio that jumps by whole,
  cross-correlated periods with a short crossfade, so splices don't click; it reads band-limited (32-tap
  sinc). A voice trails the guitar by ~3–5 ms and has the new note's interval ~8–12 ms after the pick
  attack (a voice with some DELAY gets it exactly on the attack: notes are time-stamped). Splices never
  repeat or skip a pick attack. Hammer-ons jump, bends glide.
* **CHORDS** finds the notes of a chord (up to six) and gives every partial the interval of the note it
  belongs to (Signalsmith Stretch with a frequency map). It is about 64 ms behind (shorter windows start getting chords wrong).

`tools/harmony/bench.cpp` (`tools/harmony/run.sh`) is its test bench: synthetic guitar parts (held notes over the
neck, picked phrases, fast runs, legato, bends, low riffs, a distorted amp, chords, every sample rate) with
pitch, lag, click, level-ripple and CPU numbers.

## Themes

Settings → Interface → Theme: Obsidian, Matte, Midnight, Gothic, Roman, Norse, Medieval, Neon, Sakura and
Inferno. Themes change the colours, accent and fonts, and add a decorative band and a moving layer (fog,
gold dust, snow and aurora, torchlight, synthwave grid, petals, embers). **Moving details** switches the
animation off. To add a theme, add one entry to `THEMES` in `ui/src/themes.js`.

## Your files: `Documents\Obsidian Amp Suite`

(Older builds used `Documents\Obsidian Suite`; its files are copied over once on first start.)

```
Presets\    your presets (.obsidian, plain JSON; share them as files)
Captures\   .nam files (amps and pedals)
IRs\        cab impulse responses (.wav)
Amps\       one .json per amp you want in the ADD AMP list (the amp pack's 30 amps are here too)
Faces\      the amp pack's amp faces (positions + images)
Cabs\       one .json per cab you want in the ADD CAB list
Captures\Pedals\<Pedal>\   the pedal pack's captures (Chiron, Emperor, Graven, Maw, XXXIII, Helios, Aeon)
IRs\Pedals\<Pedal>\        the pedal pack's reverb IRs (Crypt, Ossuary, Halcyon)
```

### The celestial pedals

Ten pedals in `CelestialModules.cpp`. The five reverbs share one engine, *LushTank*: a "figure-8" tank in the style of
the classic studio plate algorithms (two cross-fed branches of modulated allpasses, delays and damping) that is
smoother and denser than the FDN behind Crypt and Seraph. Faces: `visuals/pedals/<key>.json` (+ `visuals/tools/art_<key>.py`).

| Pedal | What it does | Knobs |
|---|---|---|
| Elysium Hall | huge cathedral hall: dense early reflections, long moving tail; INFINITE holds the tail while you play over it | Decay, Size, Pre-delay, Tone, Low Cut, Mod, Mix, Infinite |
| Empyrean Shimmer | two pitch voices (octave + 5th/4th/2 oct/sub/3rd) rising inside the tail | Decay, Size, Shimmer, Voice II, Interval, Tone, Mod, Mix |
| Nimbus Cloud | notes dissolve into a drifting haze; Swell fades the reverb in under each note; FREEZE holds the cloud | Decay, Cloud, Drift, Swell, Tone, Low Cut, Mix, Freeze |
| Vesper Choir | the tail sings through vowel formants (A E I O U), optional octave/fifth voices | Decay, Size, Vowel, Choir, Voices, Breath, Tone, Mix |
| Halo Reverse | REVERSE: reverb played backwards, swelling into the next note; BLOOM: the tail re-enters and builds | Mode, Length, Decay, Tone, Mod, Low Cut, Mix |
| Astral Shimmer Delay | repeats climb an octave/fifth and smear into a wash | Time, Feedback, Shimmer, Interval, Diffuse, Mod, Tone, Mix |
| Mnemosyne Tape Echo | three tape heads (7 combinations), wow/flutter, tape age, spring reverb; runs away past 95 % | Time, Intensity, Heads, Wow, Age, Spring, Echo, Tone |
| Gemini Dual Delay | two heads in a rhythm (dotted, half, triplet, third, 1.5x, golden), cross-fed; head B can play backwards | Time, Pattern, Feedback, Cross, Tone, Mod, Mix, Reverse |
| Lyra 12-String | adds a chiming octave string and a detuned twin, strummed a hair apart | Octave, Detune, Spread, Chime, Width, Mix |
| Icarus Octave Fuzz | rectifier octave-up fuzz, 4x oversampled | Fuzz, Octave, Tone, Level, Mix |

`"Obsidian Amp Suite.exe" --render-pedal-demos <folder>` renders a short clean-guitar demo through each of them
(plus wet-only impulse responses of the reverbs). The self-test checks stability with every knob at max, decaying
and frozen tails, that the shimmers really rise an octave, the choir vowels, the reverse swell, echo timing,
Icarus's octave, other sample rates and CPU.

### The pedal pack

Pack pedals bring their own files. They live in the repo in `plugin/Content` (credits in
`plugin/Content/README.md`), and the installer's **Pedal pack** option copies them into
`Documents\Obsidian Amp Suite` (it never overwrites files that are already there). The release zip has them in
`Content\`: copy its `Captures` and `IRs` folders into `Documents\Obsidian Amp Suite` by hand. When you add one of these pedals, its whole
set loads in the background and the pedal's switches pick which file plays. A missing file shows a message
and that position plays dry. The exact file for every switch position is in `visuals/registry.json`.

| Pedal | Files | Switches |
|---|---|---|
| Chiron | 2 Centaur OD captures | Capture MID / HIGH; Drive, Blend, Output |
| Emperor | 3 Duke of Tone captures | Mode BST / OD / DIST |
| Graven | 3 Fortin TS-9 captures | Capture I / II / III |
| Maw | 2 Fortin Grind captures | Capture SLAM / KODY |
| XXXIII | 1 Fortin 33 capture | – |
| Helios | 9 SD-1 captures | Voice B I–III, D I–VI |
| Aeon | 56 Alpha Omega Ultra captures | Voice ALPHA / OMEGA × Growl × Bite × Drive 0–10 (7 steps) |
| Crypt | 6 Boss RV-6 IRs | Voice: CRYPT (its own reverb) or SPRING, DELAY, HALL, PLATE, ROOM, MOD |
| Ossuary | 9 Green-Wood Catacombs IRs | Room CLOSE / MID / FAR × Mic I / II / III |
| Halcyon | 2 ambient IRs | Mode SLAP / VERB |

The IR reverbs' Decay knob shortens the IR (the tail fades out), Tone is a high cut and Low Cut a low cut on
the reverb only. Capture pedals cross-fade when you switch, so you can flip them while playing.

**Captures from TONE3000** now come as *SlimmableContainer* files. They load everywhere (NAM Loader amp,
NAM Pedal, pack pedals). `NamEngine.cpp` registers every NAM model type itself; without that the linker
dropped SlimmableContainer support and such captures were rejected.

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

**Add a pedal with its own captures or IRs**: one `addCapturePedal (…)` / `addIrPedal (…)` call in
`PackModules.cpp`: its knobs, which knobs are switches (`selectors`, most significant first) and the file list
in switch order. The self-test checks that every switch position maps to exactly one file.

**Add a UI command**: one entry in `UiBridge::buildCommands()` and call it from JS with `call('name', …)`.

**Remove something**: delete its line. Presets that still mention it simply ignore it.

**Change the UI**: edit `ui/src/*` and run `python ui/src/build.py` (rebuilds `ui/obsidian-demo.html`).
Live editing without rebuilding the plugin: set *uiDevFolder* in `settings.xml` (or the environment
variable `OBSIDIAN_UI_DEV`) to your `ui` folder and reopen the plugin window. Files found there win over the
built-in copies (copy `WebUI/obsidian-bridge.js` in too if you're editing the bridge).

**Factory presets**: the sounds are written in `tools/presets/factory.py` (the Factory folder) and
`tools/presets/artists.py` (one `artist("Name")` block per artist folder). Run
`python3 plugin/tools/presets/make_presets.py [path to the standalone]`: it builds every preset on the real
plugin, plays a test riff through it and sets the output so they all play about equally loud (needs the amp
and pedal packs). Or save a preset in the plugin and copy the file into `Presets\Factory\` (rename
`.obsidian` to `.json`). File names must be unique across all folders.

**Tone check**: `tools/presets/tonecheck.py` measures every preset on a real guitar DI (the plugin renders it with
`OBS_DI=<48 kHz wav> OBS_RENDER_DIR=<folder>` set while writing presets; `OBS_ONLY="a|b"` limits the run). It
reports lows, low-mid mud, mids, bite (2-5 kHz) and fizz against full-rig captures made by tone creators, flags
dull / muddy / fizzy / boomy / thin presets, and with `--fix` writes per-preset EQ corrections to `tonefix.json`,
which `make_presets.py` lays on top of each preset's own EQ. Run it again on fresh renders to refine them. The
Pantera presets are tuned by hand (listed in `KEEP`).

**Version**: change `VERSION`.

## Tests

* `"Obsidian Amp Suite.exe" --selftest report.txt` renders every pedal in every slot with random settings,
  every sample rate and buffer size, checks transpose pitch accuracy, the synth, AETHER (every bed, voice and texture, random worlds, FREEZE, FOLLOW), HYDRA (pitch of every voice,
  how fast it follows, bypass, moving it while playing), presets, DAW state and CPU. Add `--nam file.nam` to test a capture, and `--packs` to play every file of the installed pedal and amp
  pack (every capture amp: all its captures load, the gain is swept, every button is pressed, level and CPU).
  The GitHub build runs it on every push.
* `python plugin/tools/ui-test/run.py` runs the real UI in headless Chrome against a fake engine and clicks
  through every page (screenshots in `tools/ui-test/out`). Refresh its data with
  `"Obsidian Amp Suite.exe" --dump-ui-state plugin/tools/ui-test/state.json`.
* `"Obsidian Amp Suite.exe" --render-sounds <folder> <spec.json>` renders any list of settings (and MIDI notes)
  to WAV, for sound design. `python -I plugin/tools/presets/aether_check.py <folder> [--fix]` uses it to
  measure the loudness of every AETHER scene against its folder's target (and with `--fix`, sets their LEVEL).
* `"Obsidian Amp Suite.exe" --render-synth <folder>` renders synth patches to WAV; `python plugin/tools/amp-lab/synthcheck.py <folder>`
  measures their level, DC and aliasing. `plugin/tools/amp-lab` also has the amp measurement tools (`amplab.cpp`, `level.cpp`).

## STUDIO (standalone app)

The footer's STUDIO is a small recorder (`Source/Core/Studio.h`):

* **REC** records what you hear onto the armed track: the finished sound at the end of the chain,
  before the click and before the studio's own playback. **COUNT-IN** gives one bar of clicks first.
* Takes are moved earlier by the interface's round-trip latency (and the plugin's own, when latency
  compensation is on), so what you played lines up with what you heard.
* Every take is also saved as a 24-bit WAV in `Documents\Obsidian Amp Suite\Recordings`.
* Takes play back with each track's volume, pan, mute and solo; loop and click work while playing.
  Move, trim, split, duplicate and delete takes on the timeline; the engine follows.
* **EXPORT** mixes every track down to a 24-bit WAV.
* The page owns the arrangement and sends it on every change (`studioSync`). The engine records
  through a lock-free FIFO and never allocates or waits on the audio thread.

## Not done yet

* **STUDIO** records what you hear, not the dry guitar, so takes can't be re-amped yet. The arrangement
  (which takes sit where) lasts while the app is open. The takes themselves are kept as WAV files.
* Overdrive, Clean Boost, Compressor, Glue, Limiter, Sweep Filter, Looper and NAM Pedal have no design yet,
  so they show as plain cards.
* The in-house amps show a plain panel (with each amp's own slight tint); only the capture amps have faces.

Licences of the libraries used: [LICENSES.md](LICENSES.md).
