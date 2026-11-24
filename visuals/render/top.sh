#!/bin/bash
# Straight-down 3D views of every pedal for the plugin's board: renders/top/<key>_off.webp and _on.webp.
# Renders only what is missing, so it is safe to re-run after a restart. Pass keys to render just those.
cd "$(dirname "$0")/.."
mkdir -p renders/top
specs=$( { echo ../plugin/tools/faces/pedals/nampedal.json; for k in lament aeon mnemosyne shatter legion ascend; do echo pedals/$k.json; done; ls pedals/*.json; } | awk '!s[$0]++')
[ $# -gt 0 ] && specs=$(for k in "$@"; do [ -f pedals/$k.json ] && echo pedals/$k.json || echo ../plugin/tools/faces/pedals/$k.json; done)
for f in $specs; do
  k=$(basename $f .json)
  for st in off on; do
    [ -f renders/top/${k}_$st.webp ] && continue
    python3 render/render.py -- $f top $st 2>&1 | grep -E "rror"
    python3 render/top_finish.py $f $st && echo "TOP $k $st"
  done
done
echo TOPDONE
