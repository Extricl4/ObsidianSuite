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
```

## Engraved pedals (2026-11-21)
New pedal art is drawn by `tools/art_<key>.py` (shared helpers in `tools/pedal_art_lib.py`: 3x supersampled plate, tapered strokes, vine scrolls, leaves, thorns).
Add `"engrave": {rough, metal, depth, strength, distance}` to a pedal spec to cut the art into the surface instead of printing it on top.
Wide pedals set `size`, `res` and `cam` (see `pedals/aeon.json`).

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
