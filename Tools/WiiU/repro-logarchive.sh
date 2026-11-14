#!/bin/bash
# Reproduce the startup freeze while archiving a large previous log (Docs/WiiU-GX2-Plan.md).
# Puts the chosen files of the saved card state ($C) on Cemu's SD card and runs build $B.
#   STATE     which files to copy: any of "log gz tmp" (default all three)
#   NOREPACK  set to run the last repacked bundle as-is
#   C         saved card state (default $WIIU_TEST_DIR/console-bench/final/crash-startup)
R=$(cd "$(dirname "$0")/../.." && pwd)
T=${WIIU_TEST_DIR:-$HOME/jazz2-wiiu-test}
P=$T/cemu/squashfs-root/usr/bin/portable; S=$P/sdcard/wiiu/apps/Jazz2
C=${C:-$T/console-bench/final/crash-startup}
export B=${B:-_build_wiiu_gx2}
[ -n "$NOREPACK" ] || "$R/Tools/WiiU/repack.sh" || exit 1
mkdir -p "$T/out" "$S"
rm -f "$T"/out/* "$P/log.txt" "$S/Jazz2.log" "$S/Jazz2.log.gz" "$S/Jazz2.log.gz.tmp"
cp "$C/Jazz2.config" "$S/"
for f in ${STATE:-log gz tmp}; do case $f in log) cp "$C/Jazz2.log" "$S/";; gz) cp "$C/Jazz2.log.gz" "$S/";; tmp) cp "$C/Jazz2.log.gz.tmp" "$S/";; esac; done
ls -la "$S" | grep Jazz2
docker run --rm -v "$T":/t -v "$R":/src -u $(id -u):$(id -g) -e HOME=/tmp -e SHOTS="${SHOTS:-30 60}" cemu-xvfb /src/Tools/WiiU/cemu/run.sh
echo "--- after:"; ls -la "$S" | grep Jazz2
