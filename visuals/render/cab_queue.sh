#!/bin/bash
# renders every missing straight-on view of the real-world cabs (specs with "builder": "cab"); safe to re-run
# usage: render/cab_queue.sh [keys...]     (PY=python-with-bpy, default python3)
cd "$(dirname "$0")/.."
PY=${PY:-python3}
if [ $# -gt 0 ]; then specs=$(for k in "$@"; do echo cabs/$k.json; done); else specs=$(grep -l '"builder": "cab"' cabs/*.json); fi
for f in $specs; do
  k=$(basename $f .json)
  [ -f out/front/$k.png ] && continue
  $PY render/cab.py -- $f front 2>&1 | grep -E "RENDERED|rror"
done
echo CABQUEUEDONE
