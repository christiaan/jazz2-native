#!/bin/bash
# Configure (first time) and build a Wii U build directory in the devkitPPC image.
#   B    build directory under the repo (default _build_wiiu_gx2)
#   RHI  NCINE_PREFERRED_RHI for the first configure (GX2, the only backend since the Software one was removed)
#   LOG  full build log (default $WIIU_TEST_DIR/build.log); only errors are printed
R=$(cd "$(dirname "$0")/../.." && pwd)
T=${WIIU_TEST_DIR:-$HOME/jazz2-wiiu-test}
B=${B:-_build_wiiu_gx2}
RHI=${RHI:-GX2}
LOG=${LOG:-$T/build.log}
mkdir -p "$T"
docker run --rm -v "$R":/src -u $(id -u):$(id -g) -e HOME=/tmp devkitpro/devkitppc:latest bash -c "
cd /src && ( [ -f $B/CMakeCache.txt ] || cmake -B $B -D CMAKE_TOOLCHAIN_FILE=\$DEVKITPRO/cmake/WiiU.cmake -D CMAKE_BUILD_TYPE=Release -D NCINE_PREFERRED_RHI=$RHI -D NCINE_CONTENT_DIR=/src/Content ) && cmake --build $B -j\$(nproc)" > "$LOG" 2>&1
rc=$?
grep -E "error|Error|undefined reference" "$LOG" | grep -v "Werror\|error_\|errors\.\|Error\.cpp\|ErrorCode" | head -40
echo "exit $rc"; tail -3 "$LOG"
exit $rc
