# Obsidian Suite – visuals toolkit

3D renders (Blender / `bpy`) of every amp head, cab and pedal for the Obsidian amp-sim suite.

## Layout
- `amps/`, `cabs/`, `pedals/` – **one spec file per item** (edit only the one you're tweaking)
- `art/` – shared textures (panel labels, grille masks, logos, screens)
- `render/` – `head.py` (amps & cabs), `render.py` (pedals), fonts
- `registry.json` – every item: name, role, matching cab, cab size
- `out/front/`, `out/angle/`, `out/pedals/`, `out/preview/` – renders (not tracked)
- `legacy/` – the old one-off generator scripts, kept only for reference

## Commands
```
python3 render/head.py   -- amps/nyx.json front        # straight-on, plugin view (~1–2 min)
python3 render/head.py   -- amps/nyx.json front prev   # quick preview (~20 s)
python3 render/head.py   -- cabs/nyx_cab.json angle    # 3/4 showroom view
python3 render/render.py -- pedals/seraph.json [prev]
python3 render/render.py -- pedals/seraph.json top off # straight down, transparent, switched off (plugin board)
render/top.sh [keys]                                   # every missing pedal top view -> renders/top/<key>_off|on.webp
```

## Real-world IR cabs (2026-11-25)
`render/cab.py` builds cabs that look like the real boxes the IRs were recorded from (Mesa, Marshall, Bogner, Diezel,
Orange, Vox, ENGL, Zilla, bass cabs ...), measured in centimetres: slant or straight fronts, tolex (bronco, levant,
taurus, carpet, duratex, brushed metal), grille cloth (`art/weave_<cloth>.png`: jute, basket, checker = salt & pepper,
diamond = Vox, wheat = Orange, silver = blackface Fender, stripe = tweed era) or perforated metal over a scrim (`art/perf_<holes>.png`), piping, a reveal gap, metal /
plastic / chrome corners, handles, casters, speakers behind the cloth, the tesseract emblem (a real 4D hypercube projected
to the front) and the album cab's display, button and light lines. The spec format is at the top of `cab.py`; specs
have `"builder": "cab"` (head.py's queue skips them).
```
python3 render/cab.py -- cabs/atlas_cab.json front        # straight-on, transparent: out/front/atlas_cab.png + _map.json
python3 render/cab.py -- cabs/atlas_cab.json front prev   # quick preview
python3 render/cab.py -- cabs/atlas_cab.json angle        # 3/4 check of the shape (not used in the plugin)
render/cab_queue.sh [keys]                                # every missing front view (PY=<python with bpy>)
python3 tools/cab_weave.py [cloth] | perf                 # remake the cloth / perforated-metal tiles
python3 ../plugin/tools/faces/build_cabs.py [keys]        # renders -> the plugin's CAB page pictures
```
Each cab's sound (IRs, voice buttons) lives in `plugin/Content/Cabs/<key>.json`; see `plugin/Content/README.md`.
Fender-style and tweed cabs (2026-11-26): `top_handle` is a strap over the top, tolex `tweed` lacquered tweed
(`art/weave_tweed.png`, a 2/2 twill), cloths `silver` (blackface) and `stripe` (tweed era). Plain fronts: no knobs.

## Engraved pedals (2026-11-21)
New pedal art is drawn by `tools/art_<key>.py` (shared helpers in `tools/pedal_art_lib.py`: 3x supersampled plate, tapered strokes, vine scrolls, leaves, thorns).
Add `"engrave": {rough, metal, depth, strength, distance}` to a pedal spec to cut the art into the surface instead of printing it on top.
Wide pedals set `size`, `res` and `cam` (see `pedals/aeon.json`).

## Pedal top views for the plugin (2026-11-24)
`render.py ... top [off|on]` renders a pedal straight down with an orthographic camera and a transparent background,
the picture exactly the enclosure (60 px/cm, `top_px` / `top_samples` / `top_led` in a spec override it), lit from the
upper left. `off` leaves the LED dark and skips the glow layer. `render/top.sh` renders every pedal (and the plugin's
WRAITH design) to `renders/top/<key>_off.webp` / `_on.webp`; `plugin/tools/faces/build_pedals.py` uses those
instead of its flat drawing. Specs may also have `screens` (bezel plus dark glass, faintly lit when on).

## Finished amps (2026-11-21)
Amps with their captures wired in live in `finished/` (see `finished/README.md`): `amps_data.py` says which
capture every button and knob plays, `build_page.py` builds the interactive page from layer renders
(`head.py ... out=on|off|ptr|led`), and `export_amps.py` writes the plugin's amp files. The captures are in
`plugin/Content/Captures/Amps/`. `head.py` gained: knob/button/LED/screen/power hotspots in `map.json`,
`layer=ptr|led` and `off` renders, zone `tilt`, `chamfer`, `recess_z`, panel `slab_bevel` and `rails`,
open-chassis `can` transformers, tube collars and feet.

## Tweak loop
1. Collect several changes for one item.
2. Edit its spec (and art texture if a label or mask changes).
3. Make a preview, then a final front render.
4. Add it as a new version on that item's page in the Amp, Cab or Pedal Workshop.

## Approved look → plugin asset pack (next step)
Render the face without knobs, plus a knob filmstrip and a layout JSON (knob positions and ranges).
The Obsidian demo and the JUCE plugin both use these, so the knobs turn.

## Sound (plugin phase)
`rig.json` maps each amp to a `.nam` capture and each cab to an IR. Changing the sound never needs a re-render.
