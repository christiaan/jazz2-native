# Wii U: native GX2 rendering backend (plan and hand-off)

Status (2026-11-13): branch `wiiu-port` has a working Wii U port that renders with the `Software` RHI
and presents through wiiu-sdl2's GX2 renderer (see the Wii U section of `Docs/Consoles.dox`). It boots
to the main menu in Cemu 2.6 and has not been tested on hardware yet. The owner wants full quality,
which needs a native `GX2` RHI backend. Nothing of the backend exists yet. Milestones 1 and 2 (the two
spikes) are done; their results are under "Spike results" below. Next step: milestone 3.

## Decisions

- **Shaders are compiled offline** with CafeGLSL's standalone compiler
  (<https://github.com/Exzap/CafeGLSL>). There is no public Docker image (`ghcr.io/exzap/cafeglsl`
  does not exist); use the Linux release
  `https://github.com/Exzap/CafeGLSL/releases/download/v1.0.0-rc1/cafeglsl-1.0.0-rc1-linux-x86_64.tar.xz`,
  which runs on the host and needs only libz:
  `bin/glslcompiler -vs a.vert -ps a.frag -o out.gsh [-v] [--uniform-mode auto|register|block]`
  (`-v` prints the R600 disassembly). The tarball also has `libcafeglsl.a` and its headers. The runtime
  `glslcompiler.rpl` is not used: it would need a wut fork (unmerged wut PR 388) and an extra file on
  every player's SD card.
- **The generated shaders are committed**, like the other offline targets. Add `--emit-gx2` to
  `Sources/Utilities/ShaderCompiler`, which writes `Sources/Shaders/Generated/Gx2GeneratedShaders.h`
  with the `.gsh` (GFD) bytes per program variant plus the reflection data. The game build never runs
  the compiler. Feed CafeGLSL the GLSL the tool already produces for the GL targets, not Cg.
  `CompileRsxMicrocode` (`ShaderCompiler/Main.cpp`, around line 1886) is the model for calling an
  external compiler, with a `--cafeglsl <path to glslcompiler>` option.
- **Use uniform-block mode**: compile with `GLSL_COMPILE_BLOCK` or AUTO and call
  `GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK)`. Register mode only allows 1024 scalar components.
  CafeGLSL puts loose uniforms into a block at binding 0.
- **Template: the RSX backend** (`Sources/nCine/Graphics/RHI/RSX`, about 7.1k lines). It is
  programmable, has precompiled per-variant shaders, explicit command submission and its own VRAM
  suballocator (`RsxVram`). Use GXM (`RHI/GXM`) as the reference for the `SdlGfxDevice` wiring, where
  SDL2 keeps input and events while the RHI owns the display.

## Spike results (milestones 1 and 2, 2026-11-13)

Spike code and tools are kept outside the repo in `~/jazz2-wiiu-test/spikes/`.

### Milestone 1: render on SDL's GX2 setup

Decision: **keep `SDL_INIT_VIDEO` and the SDL window, create no `SDL_Renderer`, and render with the
backend's own `GX2ContextState`.** This is the GXM pattern (window flags 0, no renderer).

What wiiu-sdl2's video driver does (`src/video/wiiu/SDL_wiiuvideo.c`, `SDL_wiiu_gfx_heap.c` on the
`wiiu-sdl2-2.32` branch):
- `WIIU_VideoInit` calls `ProcUIInitEx` (only if ProcUI is not running yet), `GX2Init` with its own
  command buffer pool, and picks the TV render mode from `GX2GetSystemTVScanMode()` (480p, 720p or
  1080p). In its foreground-acquire callback it allocates the TV and DRC scan buffers (double
  buffered, `UNORM_R8_G8_B8_A8`) and calls `GX2SetTVBuffer`/`GX2SetDRCBuffer`. `GX2SetTVScale` and
  `GX2SetDRCScale(854, 480)` are set once.
- **SDL takes all of MEM1 and the whole foreground heap** (`MEMAllocFromFrmHeapEx` with the full
  allocatable size, wrapped in expanded heaps), and installs `GX2RSetAllocator` so that GX2R resources
  with `BIND_COLOR_BUFFER`, `BIND_DEPTH_BUFFER`, `BIND_SCAN_BUFFER` or `USAGE_FORCE_MEM1` come from that
  MEM1 heap and everything else from `memalign` (MEM2). So the backend must allocate through GX2R
  (`GX2RCreateSurface`, `GX2RCreateBuffer`), not from `MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1)`.
  `Gx2Memory` is then mostly a thin layer over GX2R.
- `SDL_PumpEvents` drives ProcUI. On `PROCUI_STATUS_RELEASE_FOREGROUND` it queues
  `SDL_APP_WILLENTERBACKGROUND`/`DIDENTERBACKGROUND` and calls `ProcUIDrawDoneRelease()` only on the
  *next* pump, so the app gets a chance to react first. The release callback then calls `GX2DrawDone`,
  frees the scan buffers and **destroys the MEM1 heap**; on acquire it rebuilds both and sends
  `SDL_APP_WILLENTERFOREGROUND`/`DIDENTERFOREGROUND` (these two are also sent once at startup).
  `PROCUI_STATUS_EXITING` becomes `SDL_QUIT`, and `SDL_VideoQuit` finishes the ProcUI exit. HOME-button
  exit therefore keeps working exactly as in the current Software port.
- The Software port presents in `WIIU_SDL_RenderPresent` (`src/render/wiiu/SDL_rpresent_wiiu.c`). Without
  an SDL renderer the backend does that itself, and SDL does not touch GX2 otherwise. One loss: the
  system software keyboard (`WIIU_SWKBD_Draw`) is drawn by the SDL renderer's present, so text input
  through it would need the backend to call it (it is an internal SDL symbol). The game does not need
  it today.

Verified in Cemu with a standalone program (`~/jazz2-wiiu-test/spikes/gx2-sdl/main.c`, built with the
devkitppc image and wut's CMake, run with its `run.sh` in the `cemu-xvfb` container):
`SDL_Init(VIDEO|AUDIO|JOYSTICK|GAMECONTROLLER)`, `SDL_CreateWindow(..., 1280, 720, 0)`, then
`GX2SetupContextStateEx(ctx, TRUE)`/`GX2SetContextState`, a 1280x720 colour buffer from
`GX2RCreateSurface` (landed at `0xF4000800`, i.e. MEM1 through SDL's allocator; a GX2R vertex buffer
landed in MEM2), and per frame:
`GX2ClearColor`, `GX2SetContextState(ctx)` (the clear clobbers the state), `GX2CopyColorBufferToScanBuffer`
for `GX2_SCAN_TARGET_TV` and `_DRC`, `GX2SwapScanBuffers`, `GX2Flush`, `GX2WaitForFlip`, and
`GX2SetTVEnable(TRUE)`/`GX2SetDRCEnable(TRUE)` after the first frame. The screen showed the cycling clear
colour at a steady 60 fps (swap interval 1). Not verifiable in Cemu: `SYSLaunchMenu()` did nothing there
(no ProcUI release or exit), and there is no HOME menu, so the background/foreground round trip has to
be tested on the console.

Consequences for the backend:
- Stop submitting GX2 work on `SDL_APP_WILLENTERBACKGROUND` and resume on `SDL_APP_DIDENTERFOREGROUND`.
  `MainApplication::ProcessEvents` (SDL2 arm) does not handle these events yet; it needs to forward
  them to the device (and probably suspend the app).
- MEM1 resources are lost in the background. Either destroy them on `WILLENTERBACKGROUND` (before
  SDL's deferred `ProcUIDrawDoneRelease`) and recreate them on `DIDENTERFOREGROUND`, or create render
  targets with `GX2R_RESOURCE_USAGE_FORCE_MEM2` so they survive. Suggested start: everything in MEM2
  (simple), then move the main colour/depth targets to MEM1 with recreation once it works, and measure.

### Milestone 2: CafeGLSL on our shaders

DefaultSprite, DefaultBatchedSprites, Combine and Blur compile **unmodified** from the GL GLSL in the
generated headers (the `WITH_RHI_GL && !RHI_GL_PROFILE_ES2` strings). Prelude as in `GLShader.cpp`:
`#version 330\n#line 0\n`; batched programs also get `RenderResources.cpp`'s
`#ifndef BATCH_SIZE / #define BATCH_SIZE (N) / #endif / #line 0` in front.

- Auto mode picks uniform-block mode for all of them (the vertex shaders have explicit std140 blocks;
  `--uniform-mode register` is rejected with "Explicit uniform blocks cannot use register mode"), so
  `GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK)` everywhere.
- **Uniform blocks are limited to 64 KB.** DefaultBatchedSprites with the in-shader default
  `BATCH_SIZE (682)` fails: `Uniform block InstancesBlock too big (76384/65536)` (682 x 112-byte stride).
  `--emit-gx2` must compute the batch size from the reflected `InstanceStride` (65536 / 112 = 585 here)
  and bake it in, and the device must publish the same value as its max batch size.
- Loose uniforms go into a block named `__cafe_loose_uniforms` at block index 0 of their stage; explicit
  blocks follow from index 1. VS and PS blocks are numbered separately. Layout found:

  | Program | VS blocks | PS blocks | PS samplers |
  |---|---|---|---|
  | DefaultSprite | loose 0 (128 B: `uProjectionMatrix` @0, `uViewMatrix` @64 B); `InstanceBlock` 1 (108 B) | none | `uTexture` 0 |
  | DefaultBatchedSprites | loose 0 (same); `InstancesBlock` 1 (65516 B at BATCH_SIZE 585) | none | `uTexture` 0 |
  | Combine | loose 0; `InstanceBlock` 1 (104 B) | loose 0 (64 B: `uAmbientColor`, `uTime`, `uWaterLevel`, `uWavePhase`) | `uTexture` 0, `uTextureLighting` 1, `uTextureBlurHalf` 2, `uTextureBlurQuarter` 3 |
  | Blur | loose 0; `InstanceBlock` 1 (108 B) | loose 0 (32 B: `uPixelOffset`, `uDirection`) | `uTexture` 0 |

  Explicit block member offsets match the std140 offsets ShaderCompiler already reflects. The reported
  block size omits trailing padding (108 instead of 112), so size buffers from the std140 stride.
  Unused uniforms are optimised out (Combine's `uCameraPos` has no entry), so the reflection data has
  to come from the compiled shader, not the source.
- No vertex attributes in these programs (`attribVarCount` 0): the sprites build their quad from
  `gl_VertexID`, which works (R0.x). The fetch shader for zero attributes still has to be checked with
  `GX2InitFetchShaderEx` on the device side.
- The batched `.gsh` is 191 KB because CafeGLSL lists every array element
  (`InstancesBlock.instances[i].*`, 2925 uniform vars); the others are 2-4 KB. `--emit-gx2` should keep
  only the block-level reflection it needs, not embed the whole GFD reflection.
- Not tested yet: the other variants (`Combine_DITHER_*` etc.) and the remaining programs.

Tools in `~/jazz2-wiiu-test/spikes/cafeglsl/`: `rel/` (the release), `extract.py` (pulls the GLSL out of
the generated headers), `dump.cpp`/`dump` (prints the reflection through `libcafeglsl.a`; run from `t/`
as `../dump Name...`), and `t/` with the test sources, `.gsh` outputs and disassembly logs.

## Backend contract (what to write)

- `RhiFwd.h`: a `WITH_RHI_GX2` arm with capability macros (`RHI_CAP_SHADERS`, `_FRAMEBUFFERS`,
  `_BATCHING`, `_HEAVY_RESCALE_SHADERS` and the rest; see the RSX arm for what to leave out and why),
  forward declarations and the `nCine::RHI::` aliases (`Device`, `Texture`, `Buffer`, `Shader`,
  `ShaderProgram`, `ShaderUniforms`, `ShaderUniformBlocks`, `Uniform`, `UniformBlock`, `UniformCache`,
  `UniformBlockCache`, `Attribute`, `Framebuffer`, `Renderbuffer`, `RenderTarget`, `VertexArray`,
  `VertexFormat`, `Capabilities`, `Debug`). Then the matching `#elif` in `Rhi.h`.
- Files, mirroring RSX: `Gx2Device` (state, clear, draws, fences, swapchain, present), `Gx2Buffer`,
  `Gx2Texture` (with `GX2Surface` tiling via `GX2CalcSurfaceSizeAndAlignment` and `GX2Invalidate` after
  CPU writes), `Gx2RenderTarget`, `Gx2ShaderProgram`/`Gx2ShaderUniforms`/`Gx2UniformCache` (load with
  wut's `WHBGfxLoadGFDShaderGroup` or `GFDGet*Shader` from memory; set up the fetch shader with
  `GX2InitAttribStream`/`GX2InitFetchShaderEx`), `Gx2VertexFormat`, `Gx2Memory` (MEM1 for colour/depth
  targets, MEM2 for the rest, 0x100 alignment), `Gx2RhiCapabilities`, `Gx2Debug`.
- `SdlGfxDevice.cpp`: four `WITH_RHI_GX2` hooks like the `WITH_RHI_GXM` ones: window flags 0, drawable
  size from `RHI::Device::GetScreenWidth/Height`, `CreateSwapchain(nullptr, w, h, ...)`, and
  `PresentFrame()` (copy the colour buffer to the TV and DRC scan buffers, `GX2SwapScanBuffers`).
- CMake: allow `NCINE_PREFERRED_RHI=GX2` on `NINTENDO_WIIU` (`cmake/ncine_options.cmake`, Wii U arm),
  define `WITH_RHI_GX2` and list the sources (`cmake/ncine_extra_sources.cmake`,
  `cmake/ncine_sources.cmake`), link `gx2`/`whb` if not already pulled in by wut. Keep `Software`
  selectable as the fallback.

## Milestones

1. **Done. Spike: SDL2 coexistence.** wiiu-sdl2's video driver initialises GX2 and owns the scan buffers.
   Find out whether the game can init SDL with only audio + joystick/gamecontroller and do its own
   ProcUI/WHBProc + GX2 init, or whether it has to render on SDL's GX2 context (`SDL_wiiuvideo.c`,
   `SDL_render_wiiu.c` in <https://github.com/devkitPro/SDL/tree/wiiu-sdl2-2.32>). HOME-button exit
   (ProcUI) must keep working. Decide before anything else.
2. **Done. Spike: CafeGLSL on our shaders.** Run the Docker compiler on 3-4 representative shaders
   (sprite, batched sprites, `Combine`, `Blur`) and check that they compile and what the uniform
   layout looks like.
3. Clear screen + present on TV and GamePad through the new device (no shaders).
4. Textured sprite program: buffers, vertex format, one texture, uniforms. Main menu visible.
5. `--emit-gx2` for all programs and variants; render targets (lighting, blur, combine, water).
6. Batching, instancing paths, fences, `RHI_CAP_*` tuning; remove the 1280x720 window cap if 1080p
   output is cheap enough.
7. Docs (`Docs/Consoles.dox`, `Docs/ShaderCompiler.dox`), CI, testing on the owner's console.

## Testing

- Build: `devkitpro/devkitppc` Docker image, `-D CMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/WiiU.cmake`.
  The previous session used `_build_wiiu/` with a wrapper script that keeps the build log out of the
  context and greps errors.
- Cemu 2.6 runs headless under Xvfb in a container (`~/jazz2-wiiu-test/`: `cemu/` extracted AppImage
  in portable mode with a preseeded `settings.xml` that skips the first-run wizard, image
  `cemu-xvfb`, `run.sh` takes screenshots, `scripts/testrun.sh` repacks and runs the current build).
  **Cemu stops at every `tw` instruction**, including the conditional `tweq` GCC 16 emits for
  libstdc++'s `shared_ptr` overflow check. That shows up as a grey screen with the main thread parked
  in an OS wait. `scripts/patch_tw.py` replaces conditional traps with `nop` in a test copy of the ELF
  before `elf2rpl`/`wuhbtool`. Real hardware is not affected.
- Cemu's GDB stub (`Cemu --enable-gdbstub`, port 1337) works with `powerpc-eabi-gdb` from the
  devkitPPC image (`--network container:<cemu container>`); breakpoints with `commands` gave the
  backtrace that found the trap.
- Game log: `sd:/wiiu/apps/Jazz2/Jazz2.log` (Cemu: `portable/sdcard/...`), and every line also goes to
  `OSReport()`, shown in Cemu's `log.txt` when the log flags are set: `<logflag>4611686018427387904</logflag>`
  (bit 62, "Coreinit Logging") in `portable/settings.xml`. Cemu logs only the format string, so pass
  an already formatted string to `OSReport`. The preseeded settings have `0`.
- Cemu's frame rate says nothing about the console's. Performance has to be judged on hardware.
