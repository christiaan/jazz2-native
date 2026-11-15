#!/bin/bash
# Repack build $B (default _build_wiiu) for Cemu: patch out the conditional traps Cemu 2.6 hangs on
# (see patch_tw.py), then elf2rpl + wuhbtool into $WIIU_TEST_DIR/jazz2.wuhb.
R=$(cd "$(dirname "$0")/../.." && pwd)
T=${WIIU_TEST_DIR:-$HOME/jazz2-wiiu-test}
B=${B:-_build_wiiu}
cp "$R/$B/jazz2.elf" "$T/jazz2.elf" || exit 1
python3 -I "$R/Tools/WiiU/patch_tw.py" "$T/jazz2.elf" "$T/jazz2_patched.elf" || exit 1
docker run --rm -v "$T":/t -v "$R":/src -u $(id -u):$(id -g) devkitpro/devkitppc:latest bash -c 'cd /t && $DEVKITPRO/tools/bin/elf2rpl jazz2_patched.elf jazz2_patched.rpx && $DEVKITPRO/tools/bin/wuhbtool jazz2_patched.rpx jazz2.wuhb --content=/src/Content --name=Jazz2Test --short-name=Jazz2Test --author=test' >/dev/null
