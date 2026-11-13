# Wii U: native GX2 rendering backend (plan and hand-off)

Status (2026-11-13): branch `wiiu-port` has a working Wii U port that renders with the `Software` RHI
and presents through wiiu-sdl2's GX2 renderer (see the Wii U section of `Docs/Consoles.dox`). It boots
to the main menu in Cemu 2.6 and has not been tested on hardware yet. The owner wants full quality,
which needs a native `GX2` RHI backend. Nothing of the backend exists yet.

## Decisions

- **Shaders are compiled offline** with CafeGLSL's standalone compiler
  (<https://github.com/Exzap/CafeGLSL>, Docker image `ghcr.io/exzap/cafeglsl:latest`,
  `glslcompiler -vs a.vert -ps a.frag -o out.gsh`, `-v` prints the R600 disassembly). The runtime
  `glslcompiler.rpl` is not used: it would need a wut fork (unmerged wut PR 388) and an extra file on
  every player's SD card.
- **The generated shaders are committed**, like the other offline targets. Add `--emit-gx2` to
  `Sources/Utilities/ShaderCompiler`, which writes `Sources/Shaders/Generated/Gx2GeneratedShaders.h`
  with the `.gsh` (GFD) bytes per program variant plus the reflection data. The game build never runs
  the compiler. Feed CafeGLSL the GLSL the tool already produces for the GL targets, not Cg.
  `CompileRsxMicrocode` (`ShaderCompiler/Main.cpp`, around line 1886) is the model for calling an
  external compiler, with a `--cafeglsl <path or docker>` option.
- **Use uniform-block mode**: compile with `GLSL_COMPILE_BLOCK` or AUTO and call
  `GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK)`. Register mode only allows 1024 scalar components.
  CafeGLSL puts loose uniforms into a block at binding 0.
- **Template: the RSX backend** (`Sources/nCine/Graphics/RHI/RSX`, about 7.1k lines). It is
  programmable, has precompiled per-variant shaders, explicit command submission and its own VRAM
  suballocator (`RsxVram`). Use GXM (`RHI/GXM`) as the reference for the `SdlGfxDevice` wiring, where
  SDL2 keeps input and events while the RHI owns the display.

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

1. **Spike: SDL2 coexistence.** wiiu-sdl2's video driver initialises GX2 and owns the scan buffers.
   Find out whether the game can init SDL with only audio + joystick/gamecontroller and do its own
   ProcUI/WHBProc + GX2 init, or whether it has to render on SDL's GX2 context (`SDL_wiiuvideo.c`,
   `SDL_render_wiiu.c` in <https://github.com/devkitPro/SDL/tree/wiiu-sdl2-2.32>). HOME-button exit
   (ProcUI) must keep working. Decide before anything else.
2. **Spike: CafeGLSL on our shaders.** Run the Docker compiler on 3-4 representative shaders
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
  `OSReport()`, shown in Cemu's `log.txt` when the log flags are set.
- Cemu's frame rate says nothing about the console's. Performance has to be judged on hardware.
