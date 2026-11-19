#!/bin/sh
# Rebuild the board from the netlist: place, route, clean up.
# Usage: tools/build_pcb.sh /path/to/freerouting.jar
# Freerouting 2.1.0 sometimes writes a .ses that is missing wire segments or
# vias while reporting every net routed (worse with its optimizer on, and
# with --keep-tracks passes). So the optimizer stays off, each attempt routes
# from scratch, and an attempt is kept only if KiCad's DRC finds zero
# unconnected items right after the import; otherwise retry with another
# pass budget. The dangling-stub cleanup in finalize.py must only ever see a
# fully connected board, or it eats half-routes back to the pad.
set -e
cd "$(dirname "$0")/.."
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
CLI=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
FR=${1:?pass the Freerouting jar path}
for MP in 100 150 80 120 60 200; do
    $KPY tools/gen_pcb.py 2>/dev/null | grep wrote
    $KPY tools/route.py export 2>/dev/null | grep dsn
    rm -f carrier.ses
    java -Djava.awt.headless=true -jar "$FR" -de carrier.dsn -do carrier.ses -mp $MP \
        --router.optimizer.enabled=false --gui.enabled=false 2>&1 | grep incomplete_count | tail -1
    $KPY tools/route.py import 2>/dev/null | grep ses
    $CLI pcb drc --format json -o drc-import.json carrier.kicad_pcb >/dev/null 2>&1
    UNC=$(python3 -c "import json; print(len(json.load(open('drc-import.json'))['unconnected_items']))")
    rm -f drc-import.json
    echo "attempt -mp $MP: $UNC unconnected after import"
    [ "$UNC" = 0 ] && break
done
[ "$UNC" = 0 ] || { echo "no clean routing attempt"; exit 1; }
$KPY tools/finalize.py 2>/dev/null | grep -vE "Debug|Fontconfig|swig|assert"
