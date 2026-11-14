#!/bin/bash
# Repack build $B (default _build_wiiu) and run it headless in Cemu, taking screenshots at $SHOTS seconds
# (default "30 60") into $WIIU_TEST_DIR/out/. The game log is in Cemu's portable/sdcard/wiiu/apps/Jazz2/.
R=$(cd "$(dirname "$0")/../.." && pwd)
T=${WIIU_TEST_DIR:-$HOME/jazz2-wiiu-test}
P=$T/cemu/squashfs-root/usr/bin/portable
"$R/Tools/WiiU/repack.sh" || exit 1
mkdir -p "$T/out"
rm -f "$T"/out/* "$P/log.txt" "$P/sdcard/wiiu/apps/Jazz2/Jazz2.log"
docker run --rm -v "$T":/t -v "$R":/src -u $(id -u):$(id -g) -e HOME=/tmp -e SHOTS="${SHOTS:-30 60}" cemu-xvfb /src/Tools/WiiU/cemu/run.sh
