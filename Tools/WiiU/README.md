# Wii U test tools

Scripts used to build the Wii U port, run it headless in Cemu and benchmark it on a console. They were
written for the GX2 backend bring-up (see `Docs/WiiU-GX2-Plan.md`, section "Testing", for what was found
with them). Nothing here is part of the game build.

Everything runs in Docker. The scripts find the repository from their own location and keep their working
files (the Cemu install, repacked bundles, screenshots, build logs) in `$WIIU_TEST_DIR`, by default
`~/jazz2-wiiu-test`.

## One-time setup

```sh
T=${WIIU_TEST_DIR:-$HOME/jazz2-wiiu-test}; mkdir -p $T/cemu $T/out

# Images: devkitPPC for building, cemu-xvfb for running Cemu under Xvfb, cemu-xvfb-dbg adds xdotool and glslang
docker pull devkitpro/devkitppc:latest
docker build -t cemu-xvfb -f Tools/WiiU/docker/cemu-xvfb.Dockerfile Tools/WiiU/docker
docker build -t cemu-xvfb-dbg -f Tools/WiiU/docker/cemu-xvfb-dbg.Dockerfile Tools/WiiU/docker

# Cemu 2.6 (Linux AppImage from https://github.com/cemu-project/Cemu/releases), extracted, in portable mode,
# with settings that skip the first-run wizard and use OpenGL
cd $T/cemu && ./Cemu-2.6-x86_64.AppImage --appimage-extract >/dev/null && cd -
mkdir -p $T/cemu/squashfs-root/usr/bin/portable/sdcard/wiiu/apps/Jazz2
cp Tools/WiiU/cemu/settings.xml $T/cemu/squashfs-root/usr/bin/portable/
```

The game data goes in `Content/` as for any build. Cemu's SD card is
`$T/cemu/squashfs-root/usr/bin/portable/sdcard/`; the game's files are in `wiiu/apps/Jazz2/` there.

## Build and run

```sh
Tools/WiiU/build.sh                                 # GX2 build in _build_wiiu_gx2/, errors only, log in $T/build.log
B=_build_wiiu RHI=Software Tools/WiiU/build.sh      # the Software backend instead
B=_build_wiiu_gx2 SHOTS="30 60" Tools/WiiU/testrun.sh   # repack, run in Cemu, screenshots in $T/out/
```

`testrun.sh` calls `repack.sh`, which copies `jazz2.elf`, patches out the conditional trap instructions Cemu
stops at (`patch_tw.py`; real hardware is not affected) and packs `$T/jazz2.wuhb` with `elf2rpl` and
`wuhbtool`. The game's `Jazz2.log` usually ends up empty, because Cemu is killed at the end of the run; see
the plan's "Testing" section for getting a log through `OSReport()` instead.

## Test hooks (never commit them applied)

- `patches/autolevel-hook.patch`: boots straight into a level. Apply with `git apply`, build, write
  `<episode>/<level> [rescale] [players] [walk] [noZoomOut]` (e.g. `prince/castle1`) to `autolevel.txt` on
  Cemu's SD card next to the game, run `testrun.sh`. Remove the file to get the intro and menu again; undo the
  patch with `git apply -R`.
- `patches/bench-hook.patch`: the console benchmark (autolevel plus a runner, GPU timers and counters). It is
  the commit on top of branch `wiiu-bench2`. Put `bench-tests-mem2.txt` next to the game, one test per line,
  `<episode>/<level> <rescale> <players> <noZoomOut> <variant>`, optionally `#seconds <warmup> <measure>`
  (e.g. `#seconds 2 3` for a short run in Cemu). Results go to `bench-mem2.txt` beside it. GPU timers only work
  on a console; Cemu's frame times say nothing about the console's. `git apply -R` also removes the new
  `Sources/WiiUBench.h`.

## Debugging in Cemu

- `cemu/run_dbg.sh` (image `cemu-xvfb-dbg`) runs `$XSTEPS` after starting Cemu with `$CEMU_ARGS` (default
  `-g /t/jazz2.wuhb`): `;`-separated xdotool commands, `sleep:N` and `shot:name` (screenshot to `$T/out/`).
  ```sh
  docker run --rm -v $T:/t -v $PWD:/src -u $(id -u):$(id -g) -e HOME=/tmp \
    -e XSTEPS="sleep:20;key Return;sleep:3;shot:menu" cemu-xvfb-dbg /src/Tools/WiiU/cemu/run_dbg.sh
  ```
  The plan's "Testing" section has the step list that turns on Cemu's shader dump.
- `cemu/rungdb.sh` starts Cemu with its GDB stub on port 1337; `cemu/bp.gdb` is an example breakpoint script.
  Give GDB the patched ELF and wait for the game to load before attaching:
  ```sh
  docker run -d --name cemu-gdb -v $T:/t -v $PWD:/src -u $(id -u):$(id -g) -e HOME=/tmp cemu-xvfb /src/Tools/WiiU/cemu/rungdb.sh
  docker run --rm -it --network container:cemu-gdb -v $PWD/Tools/WiiU/cemu:/g -v $T:/t devkitpro/devkitppc:latest \
    bash -c 'sleep 15; $DEVKITPRO/devkitPPC/bin/powerpc-eabi-gdb -q -x /g/bp.gdb /t/jazz2_patched.elf'
  docker rm -f cemu-gdb
  ```
- `repro-logarchive.sh` reproduces the open startup freeze while archiving a large previous log: it copies
  the saved card state (`$C`, default `$T/console-bench/final/crash-startup`) to Cemu's SD card and runs build
  `$B`. `STATE` picks the files (`log gz tmp`), `NOREPACK=1` reuses the last bundle.

## Other

- `scan_levels.py <dir with .j2l files> <water event id>`: lists levels with their start lighting and whether
  they set a water level (`python3 -I Tools/WiiU/scan_levels.py ~/jazz2-game/jazz2 208`).
- `spikes/`: the standalone programs from milestones 1 and 2. `gx2-sdl` checks that SDL2 and the game's own
  GX2 setup coexist, `gx2-draw` draws `DefaultSprite` from the generated GX2 header with fixed uniforms. Build
  and run:
  ```sh
  S=Tools/WiiU/spikes/gx2-draw
  docker run --rm -v $PWD:/src -v $PWD/$S:/s -u $(id -u):$(id -g) -e HOME=/tmp devkitpro/devkitppc:latest \
    bash -c 'cmake -S /s -B /s/build -D CMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/WiiU.cmake && cmake --build /s/build'
  docker run --rm -v $T:/t -v $PWD/$S:/s -u $(id -u):$(id -g) -e HOME=/tmp cemu-xvfb /s/run.sh   # shot_a/b.png in $S
  ```
  `cafeglsl/extract.py` writes four programs' GLSL to `cafeglsl/t/`; `cafeglsl/dump.cpp` prints what CafeGLSL
  (<https://github.com/Exzap/CafeGLSL>) makes of them.
- `analysis/`: scripts from tracking down Cemu's wrong colours with the `Antialiasing` program (they showed
  the bug is in Cemu's shader translation). `r600_emu_antialiasing.py <glslcompiler log> [seed]` evaluates the
  R600 disassembly, `cemu_glsl_emu_antialiasing.py <Cemu GLSL dump> <seed>` Cemu's translation of it, both
  against a reference; `scan_cemu_pvps.py Sources/Shaders/Generated/Gx2GeneratedShaders.h` lists the programs
  that hit the same Cemu bug.
