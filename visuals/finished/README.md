# Finished amps

The amps that have their captures wired in, working knobs and buttons, and power on/off.
`site/` is the interactive page (published as the claude.ai artifact "Obsidian Suite · finished amps").

| File | What |
|---|---|
| `amps_data.py` | every finished amp: skin(s), knob names, buttons, which capture each button combination plays, description |
| `build_page.py` | builds `site/` from the layer renders in `visuals/out/layers/<skin>/` |
| `page_template.html` | the page: knobs, buttons, LEDs, power, the Eidolon loader screen |
| `export_amps.py` | writes `plugin/Content/Amps/<key>.json` (the plugin's amp format + the full map) and checks every capture file exists |
| `shot.py`, `shot_eid.py` | headless test: drag a knob, press every button, power off; prints any JS errors |

The captures themselves are in `plugin/Content/Captures/Amps/<Amp>/` (credits in `plugin/Content/README.md`).

## Layers for one amp skin
```
python3 render/head.py -- amps/<skin>.json front out=on
python3 render/head.py -- amps/<skin>.json front out=off off
python3 render/head.py -- amps/<skin>.json front out=ptr layer=ptr samples=16
python3 render/head.py -- amps/<skin>.json front out=led layer=led samples=16
```
`on`/`off` are the faces without pointers, `ptr` the pointers alone, `led` the lit LEDs alone, and `map.json`
the pixel position of every knob, button, LED, screen and the power switch. The page rotates the pointer
sprites and switches the LED sprites, which is also how the plugin can draw them.

## Finished specs
Finished amps use their own spec next to the original design, e.g. `amps/lamia_be.json` (made by
`tools/finished_lamia.py` from `amps/lamia.json`). The scripts are `tools/finished*.py`, `tools/art_*.py`,
`tools/spec_eidolon.py` and `tools/new_erebus_dionysus.py` + `tools/art_dionysus.py` + `tools/dionysus_v2.py`.
The new Erebus (Überschall MKII Ultra) is `amps/erebus_uu.json`; `amps/erebus.json` is the original Erebus design.

| Amp | Skin spec | Based on |
|---|---|---|
| Moirai | `amps/moirai_green.json`, `amps/moirai_blue.json`, `amps/moirai_red.json` | EVH 5150 III 50W Ivory |
| Umbra | `amps/umbra_mk5.json` | Mesa/Boogie Mark V |
| Citadel | `amps/citadel_iic.json` | Mesa/Boogie Mark IIC+ |
| Ares | `amps/ares_pb.json` | ENGL Powerball II |
| Strix | `amps/strix_hw.json` | Schecter Hellwin 100W (Synyster Gates) |
| Nihil | `amps/nihil_fm.json` | Fortin Meshuggah / Sunn Model T |
| Talos | `amps/talos.json` | Mesa/Boogie Dual Rectifier |
| Achlys | `amps/achlys.json` | Mesa/Boogie Mark IV |
| Lyssa | `amps/lyssa.json` | Peavey Invective 120 |
| Belial | `amps/belial.json` | Randall Satan |
| Melinoe | `amps/melinoe_sr.json` | Fender Super Reverb ('77) |
| Nyx | `amps/nyx_ub.json` | Bogner Überschall Rev Blue |
| Chrysaor | `amps/chrysaor.json` | Marshall JCM800 2203 |
| Eris | `amps/eris.json` | Mesa/Boogie JP-2C (John Petrucci) |
| Kratos | `amps/kratos.json` | Peavey 6505+ |
| Vulcan | `amps/vulcan_vh4.json` | Diezel VH4 |
| Moros | `amps/moros_gp.json` | Omega Ampworks Granophyre |
| Niflheim | `amps/niflheim_rv.json` | Revv Generator 120 MkIII (red channel) |
| Persephone | `amps/persephone_jc.json` | Roland JC-120 Jazz Chorus |
| Hyperion | `amps/hyperion_ds.json` | Dumble Overdrive Special 102 ("Ford") |
| Hecate | `amps/hecate_evh.json` | EVH 5150 |
| Typhon | `amps/typhon_green.json`, `amps/typhon_blue.json`, `amps/typhon_red.json` | EVH 5150 III Stealth 100W |
| Tyche | `amps/tyche_pl.json` | Marshall JMP-50 Lead (1969 Plexi) |
| Thanatos | `amps/thanatos_slo.json` | Soldano SLO-100 (overdrive channel) |
| Orpheus | `amps/orpheus.json` | Acoustic simulator |
| Lamia | `amps/lamia_be.json` | Friedman BE-100 Deluxe |
| Astraea | `amps/astraea_ac.json` | Vox AC30 CH |
| Eos | `amps/eos_hw.json` | Hiwatt DR103, plus the Comfortably Numb solo tones |
| Erebus | `amps/erebus_uu.json` | Bogner Überschall MKII Ultra |
| Dionysus | `amps/dionysus.json` | Bogner Ecstasy 101B |
| Eidolon | `amps/eidolon_fin.json` | Loads your own NAM captures |
