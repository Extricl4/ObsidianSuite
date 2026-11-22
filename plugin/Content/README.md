# Obsidian pedal pack

The captures and impulse responses the pack pedals play. The installer copies this folder into
`Documents\Obsidian Amp Suite` (files you already have there are left alone), and each pedal loads its own
files from there. Layout and file names must stay as they are: every pedal looks its files up by name.

```
Captures\Pedals\<Pedal>\*.nam   Chiron, Emperor, Graven, Maw, XXXIII, Helios, Aeon
IRs\Pedals\<Pedal>\*.wav        Crypt (RV-6 modes), Ossuary, Halcyon
```

Which file each switch position plays: `plugin/README.md` (The pedal pack) and `visuals/registry.json`.
The self-test plays every file: `"Obsidian Amp Suite.exe" --selftest report.txt --packs`.

## Credits

These files were made by other people and downloaded by Emil for his own rig. All rights stay with their creators.

| Pedal | Files | Source | Made by |
|---|---|---|---|
| Chiron | 2 captures | Behringer Centaur Overdrive (TONE3000) | voncaps |
| Emperor | 3 captures | MXR Custom Shop Duke of Tone (TONE3000) | frank67 |
| Graven | 3 captures | Fortin-modded TS-9 Tube Screamer (TONE3000) | colossaldave |
| Maw | 2 captures | Fortin Grind (TONE3000) | slamminmofo |
| XXXIII | 1 capture | Fortin 33, Fredrik Thordendal signature (TONE3000) | obijuan |
| Helios | 9 captures | Boss SD-1 Super Overdrive (TONE3000) | ampspedalspickups |
| Aeon | 56 captures | Darkglass Alpha Omega Ultra (TONE3000) | leenykeany |
| Crypt | 6 IRs | Boss RV-6 reverb modes | (from Emil's download "Boss_RV-6") |
| Ossuary | 9 IRs | The Green-Wood Cemetery Catacombs | (from Emil's download "The_Green-Wood_Cemetery_Catacombs") |
| Halcyon | 2 IRs | Delay And Reverb Ambient Shoegaze Machine | Tone Charm Audio (free pack) |

# Obsidian amp pack

The captures the finished amps play (the amps on the finished-amps page, `visuals/finished/`).

```
Captures\Amps\<Amp>\*.nam   one folder per amp (Moirai, Umbra, ... Dionysus)
Amps\<key>.json              one file per amp, in the format the ADD AMP list reads
```

Each `Amps\<key>.json` has:
- `captures`: one default file per channel (CH 1–4), so the amp works in today's plugin.
- `map`: the full wiring from the finished-amps page. `buttons` are the amp's buttons (channel, mic, voice,
  boost...), `settings` says which files each button combination plays and how (`gain`: the gain knob steps
  and blends through the files in order; `one`: a single capture, gain drives it; `mid`: the middle knob
  blends; `grid`: two knobs blend a grid; `near`: the files were captured at real knob settings (`at`, with
  `axes` naming the knobs and `w` their weights) and the closest one plays). `selector` is Eos's SOLO switch.
  `rig: true` means the cab is inside that capture, so the cab section should be bypassed.
- Regenerate with `python3 visuals/finished/export_amps.py`. It also checks every file is here.
- Nemean and Phlegethon are the plugin's own amps (not on the finished-amps page): their `Amps\<key>.json` is written
  by hand and their faces come from `plugin/tools/faces/build_own_amps.py` (art and Blender specs in `plugin/tools/faces/amps/`).
- An amp whose `kind` starts with "Full rig" plays every setting as a full rig (the plugin's cab bypassed) unless the
  setting says `"rig": false`.

The installer's pack option installs `Captures\Amps`, `Amps\*.json` and `Faces\` (the amp faces the plugin draws,
made by `plugin/tools/faces/build_faces.py` from `visuals/finished/site`), and the release zip carries them under
`Content`. The plugin reads the "map" itself (`plugin/Source/DSP/CaptureAmp.h`).

## Amp credits

These files were made by other people and downloaded by Emil for his own rig. All rights stay with their creators.

| Amp | Files | Pack | Made by |
|---|---|---|---|
| Moirai | 28 captures | EVH 5150iii Ivory FULL Pack | ampspedalspickups |
| Umbra | 37 captures | Mesa Boogie - Mark V [18dBu] | kenazmusic |
| Citadel | 10 captures | Mesa Boogie Mark IIC+ | siamsiamguite |
| Ares | 8 captures | ENGL Powerball II, Grossmann Fatbox, Celestion Raven | kaidownguitar |
| Strix | 6 captures | Hellwin 100W 'Synyster Settings' | gtrjeremy |
| Nihil | 3 captures | FORTIN MESHUGGAH + SUNN MODEL T + FRIEDMAN BUTTERSLAX | nosatec |
| Talos | 5 captures | MESA DUAL RECTIFIER 2025 | deathblossomaudio |
| Achlys | 9 captures | Mesa Boogie Mark IV pack | petrc |
| Lyssa | 5 captures | PEAVEY  INVECTIVE | valdir0545 |
| Belial | 2 captures | REAL Olas Randall Satan 50 | olaenglund |
| Melinoe | 3 captures | Fender Super Reverb 1977 | tone3000 |
| Nyx | 16 captures | Bogner Uberschall | 2dor |
| Chrysaor | 30 captures | Marshall JCM 800 2203 | arthm |
| Eris | 19 captures | Mesa Boogie JP-2C Pack | emilrohbe |
| Kratos | 30 captures | 6505 Gain Range Pack (High Gain) | ampspedalspickups |
| Vulcan | 29 captures | Diezel VH4 (three packs merged) | deathblossomaudio · amalgamaudio · 2dor |
| Moros | 3 captures | Omega Granophyre | tonecast |
| Niflheim | 9 captures | REVV GENERATOR 120 RED MKIII | deathblossomaudio |
| Persephone | 7 captures | Roland JC120 Jazz Chorus | autoguitarrig |
| Hyperion | 17 captures | Dumble ODS 102 Ford Hyper Accuracy | slamminmofo |
| Hecate | 1 captures | EVH 5150 | rusen |
| Typhon | 3 captures | 5150 Stealth 100w Mesa OS Full Rig - Blue, Red and Green | jpisoutoftune |
| Nemean | 4 captures | Pantera Full Tone (one per album: Cowboys From Hell, Walk, I'm Broken, The Great Southern Trendkill) | hellyeahdime |
| Phlegethon | 1 capture | Black And Blue NAM Profiles: Black Go (Archetype Gojira) | cyclone |
| Tyche | 1 captures | Marshall JMP-50 Lead 1969 Plexi A2 | amalgamaudio |
| Thanatos | 1 captures | Soldano SLO-100 - Drive | amalgamaudio |
| Orpheus | 2 captures | Acoustic Sim | gloo |
| Lamia | 24 captures | Friedman BE100 Deluxe | 2dor |
| Astraea | 96 captures | VOX AC30 CH Hyper Accuracy | slamminmofo |
| Eos | 20 captures | Hiwatt Custom 100 DR103 1974 + Pink Floyd Comfortably Numb Tone Collection | daweed, frank67 |
| Erebus | 1 captures | Bogner Uberschall MKII Ultra High Gain | amalgamaudio |
| Dionysus | 8 captures | Bogner Ecstasy 101B Archive | Kenji Suzuki |
