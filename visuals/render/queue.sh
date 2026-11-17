#!/bin/bash
# renders every amp & cab front view that is missing; safe to re-run after a restart
cd "$(dirname "$0")/.."
for f in amps/*.json cabs/*.json; do k=$(basename $f .json); [ -f out/front/$k.png ] || python3 render/head.py -- $f front 2>&1 | grep -E "RENDERED|rror"; done
echo QUEUEDONE
