#!/bin/bash
# Renders the layers build_own_amps.py needs (face on / off, pointer sprites, LEDs) into out/layers/<skin>/.
# Skips what's already there, so it's safe to re-run after a restart. Needs a Python with bpy:
#   PY=/path/to/python plugin/tools/faces/amps/render.sh
cd "$(dirname "$0")"
PY=${PY:-python3}
r() { local spec=$1 out=$2; shift 2; local k=$(basename $spec .json)
      [ -f out/layers/$k/$out.png ] || $PY render/head.py -- $spec front out=$out "$@" 2>&1 | grep -E "RENDERED|rror"; }
for s in cfh vdop fbd tgst; do r nemean_$s.json on; done
r nemean_cfh.json off off
r nemean_cfh.json ptr layer=ptr
r nemean_cfh.json led layer=led
r phlegethon.json on
r phlegethon.json off off
r phlegethon.json ptr layer=ptr
r phlegethon.json ptr_off layer=ptr off
echo RENDERS-DONE
