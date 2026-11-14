# Wii U: native GX2 rendering backend (plan and hand-off)

Status (2026-11-14): branch `wiiu-port` has a working Wii U port with two RHIs: the native `GX2` backend (the
default since 2026-11-14) and `Software` (the fallback, presenting through wiiu-sdl2's GX2 renderer) (see the Wii U section of
`Docs/Consoles.dox`). Milestones 1 and 2 (the spikes), 3 (the backend skeleton with clear and present),
4 (shaders, uniforms, textures; the main menu renders), 5 (levels, render targets and all rescale shaders)
and 6A (the CPU no longer waits for the GPU every frame) are done; see "Spike results" and the
"Milestone N results" sections below. Milestone 7's docs and CI are done too. The GX2 build runs on the
owner's console (Aroma), much more smoothly than `Software`, and outputs 1080p there. Three-player
splitscreen has correct colours on the console (2026-11-14), which confirms that the wrong colours in Cemu
come from Cemu's shader translation (see "Cemu tests"). `GX2` is now the default (milestone 7 is done).
The console benchmark (2026-11-14, see "Console benchmark") favours the tile-layer window cache and batching,
and shows that four players and the 3xBRZ, SABR and CleanEdge rescale modes are GPU-bound at 1080p.

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

## Milestone 3 results (2026-11-13)

The backend exists in `Sources/nCine/Graphics/RHI/GX2/` (cloned from RSX, then rewritten where the
hardware differs) and is selected with `-D NCINE_PREFERRED_RHI=GX2`. Verified in Cemu: the game boots
through the GX2 device to the main loop and every frame is cleared and presented on the TV; a temporary
build that forced a cycling clear colour on the screen buffer showed it full screen, changing between
screenshots. Nothing else is visible yet, because no program has shader binaries (see below).

What is in place:
- `RhiFwd.h`/`Rhi.h` arms with `RHI_CAP_SHADERS`, `_FRAMEBUFFERS`, `_TRANSPARENT_RENDER_TARGETS`,
  `_HEAVY_RESCALE_SHADERS`, `_BATCHING` and `_STREAMING_TEXTURES`. Revisit `_HEAVY_RESCALE_SHADERS` in
  milestone 5 if CafeGLSL rejects a rescale shader.
- `Gx2Device`: own `GX2ContextState` (shadowed) on SDL's GX2 setup, `GX2SetShaderMode(UNIFORM_BLOCK)`,
  a 1280x720 screen `GX2ColorBuffer` (tiled, MEM2), blend/cull/viewport/scissor state, `Clear()` with
  `GX2ClearColor()` followed by `GX2SetContextState()`, `PresentFrame()` copying to the TV and DRC scan
  buffers, `GX2SwapScanBuffers`, `GX2Flush`, `GX2WaitForFlip`, TV/DRC enable after the first frame, and
  the RSX-style retire list (freed two presents later). `MaxBatchSize` 585, `GetMaxUniformBlockSize()`
  65536, uniform offset alignment 256.
- No depth buffer anywhere: the engine never enables the depth test, so depth/stencil clears are ignored
  and the depth unit is off.
- `Gx2Memory`: every allocation is MEM2 from the default heap (`MEMAllocFromDefaultHeapEx`), not GX2R;
  `FlushCpuWrites()` wraps `GX2Invalidate(mode | CPU)`.
- `Gx2BufferObject`: MEM2 store, flushed after `BufferData`/`BufferSubData`, on `FlushMappedBufferRange`
  and on `Unmap` (the last mapped range).
- `Gx2Texture`: host store as in RSX plus a GPU `GX2Texture` (RGBA8; `LINEAR_ALIGNED` for ordinary
  textures, tiled for render targets), `compMap` from the swizzle, `GX2Sampler` from filter/wrap,
  `ApplySamplerState(unit)` binds both. Written but **not exercised yet** (no draws).
- `Gx2RenderTarget`: a `GX2ColorBuffer` over the attached texture's surface; `SetRenderTarget` binds it
  at once so a clear that follows hits it.
- `Gx2ShaderProgram`: links from the offline reflection only; `HasBinaries()` is always `false`, and
  `Gx2Device::DrawCommon()` returns early for such a program. Uniform caches, uniform blocks and vertex
  formats are the RSX ones unchanged.
- Background: `MainApplication::ProcessEvents` (SDL2, `WITH_RHI_GX2` only) calls
  `RHI::Device::EnterBackground()` on `SDL_APP_WILLENTERBACKGROUND` (`GX2DrawDone`, then nothing is
  submitted) and `EnterForeground()` on `SDL_APP_DIDENTERFOREGROUND` (context state made current again),
  plus `SetFocus(false/true)`. In the background `PresentFrame()` sleeps 16 ms to throttle the loop.
  Untested: Cemu has no HOME menu; needs the console.
- `SdlGfxDevice`: window flags 0, drawable size from `GetScreenWidth/Height()`, `CreateSwapchain` after
  the window, `PresentFrame()` in the update arm, no GL context teardown. `AboutSection` shows "GX2".
- CMake: `NCINE_PREFERRED_RHI` accepts `Software` (then the default) or `GX2` on the Wii U; nothing extra to
  link (coreinit and gx2 are part of wut).

Open points for milestone 4:
- The generated shader table (`--emit-gx2`), loading GFD shaders, fetch shaders from `Gx2VertexFormat`
  (zero attributes for the sprite programs, see milestone 2), uniform blocks (loose uniforms into block 0
  from `ResolveUniform()`, explicit blocks from `GetUniformRange()`; VS and PS numbered separately), and
  textures through `ApplySamplerState()`. Uniform data handed to `GX2SetVertexUniformBlock` must be in
  flushed, 256-byte aligned memory; the loose-uniform block needs a per-draw ring of its own.
- Buffer streaming: GX2 does not rename buffers, so the CPU can overwrite vertex/uniform data the GPU
  is still reading for the previous frame. RSX solved this by defining `NCINE_HAS_PERSISTENT_MAPPING` and
  `NCINE_ALWAYS_PERSISTENT_MAPPING` in `RenderBuffersManager.cpp` (sectioned rings). The same is likely
  needed here; with persistent mapping nothing is unmapped, so the flush has to move to
  `FlushMappedBufferRange` or the draw path.
- `Gx2Texture::TexSubImage2D` rewrites texels in place; fine while the GPU waits for each flip, but
  check once frames overlap. Render-target readback (`GetTexImage`) returns the blank host store.
- Cemu's `Jazz2.log` is not flushed when `testrun.sh` kills Cemu, so late log lines are lost; use the
  Cemu log flags and `OSReport()` (see Testing) when log output matters. (Done in milestone 4: the log
  entries now reach Cemu's log as text.)

## Milestone 4 results (2026-11-13)

The main menu renders through the GX2 backend in Cemu, matching the Software build (and also drawing the
starfield background the Software tier leaves out). Frame 1 draws about 50 sprites with `TexturedBackground`,
`DefaultSprite`, `DefaultBatchedSprites`, `BatchedColorized` and `BatchedPaletteRemap` into a 720x405
render target, then one `DefaultSprite` draw scales it to the 1280x720 screen buffer. No backend warnings
in the log.

What is in place:
- `ShaderCompiler --emit-gx2 <out.h> [--cafeglsl <glslcompiler>] <in.shader ...>` (also run by
  `--generate-all`, skipped with a notice when CafeGLSL is missing, like `--emit-rsx`). It feeds CafeGLSL
  the GL 3.3 GLSL (`#version 330` prelude, `BATCH_SIZE (585)` for batched programs) with
  `--uniform-mode block`, parses the GFD file itself (tagged-offset pointers, see `ParseGfdShaderHeader`)
  and writes `Sources/Shaders/Generated/Gx2GeneratedShaders.h`: per stage the `regs` words, the program
  bytes and GX2 reflection tables (uniform blocks, loop vars, samplers, attribs, and only the members of
  `__cafe_loose_uniforms`). All 97 program variants compile; the header is 2.3 MB (RSX: 3.4 MB).
  Regenerate with `~/jazz2-wiiu-test/sc_build/ShaderCompiler --generate-all --shaders-dir Sources/Shaders
  --out-dir <tmp> --cafeglsl <glslcompiler>` and copy `Gx2GeneratedShaders.h` over (on Linux the D3D11
  and Vulkan headers come out stale, so do not use the in-tree `--generate-all` there). Run on
  `Sources/Shaders/*.shader` by hand, the entries come out in shell-glob order instead; same content.
  `--generate-all --check` on a host without CafeGLSL reports the committed header as "committed but no
  longer generated", exactly as it already does for `RsxGeneratedShaders.h` without cgcomp; the check
  needs an exemption for skipped optional headers before CI runs it.
- `Gx2ShaderProgram::Link()` looks the (program, variant) up, copies both programs to MEM2 (256-aligned),
  fills `GX2VertexShader`/`GX2PixelShader` and builds the bindings: loose uniforms (offsets are in
  32-bit units in `GX2UniformVar`), explicit blocks matched by name to the engine block whose index is
  its binding point, and samplers (engine unit from the reflection, hardware unit from the shader). A
  program missing from the table links without binaries (warning) and its draws are skipped. Batched
  programs take their batch size from the table.
- Fetch shaders are built per program on first use and cached by `Gx2VertexFormat::CalculateFingerprint()`:
  one stream per enabled attribute, attribute buffer index = engine attribute slot, the attribute's
  offset within the vertex in the stream, `GX2_ENDIAN_SWAP_DEFAULT`. The sprite programs have no
  attributes (empty fetch shader, works). **Attribute-based programs (mesh sprites, ImGui) are not
  exercised by the menu and untested.** ImGui's packed `ImU32` colour is written in CPU (big-endian)
  order, so `UNORM_8_8_8_8` will probably read it as ABGR; check when ImGui is first drawn.
- `Gx2Device::DrawCommon()`: shaders (only when the program changes), fetch shader, uniform blocks,
  textures (`Gx2Texture::ApplySamplerState()` on the hardware unit), attribute buffers, then
  `GX2DrawEx(mode, count, firstVertex, instances)` or `GX2DrawIndexedEx2(..., baseVertex, ...)`.
- **Uniform blocks must hold little-endian words.** The GPU swaps vertex attributes but reads uniform
  blocks as they are (found with a spike: the byte-swapped copy of the same quad drew, the native one did
  not). The device copies every loose block and every bound explicit range into a 4 MB per-frame ring
  (`AllocateUniformData()`) with each 32-bit word swapped. Swapping the engine's buffers in place was
  rejected: a range can be flushed twice (`FlushMappedBufferRange` and then `Unmap`).
- **The shader mode is not restored by `GX2SetContextState()`** (it is a config register), and the
  clears and scan-buffer copies use GX2's own register-mode shaders, so `RestoreContextState()` sets
  `GX2_SHADER_MODE_UNIFORM_BLOCK` again after each of them. (This alone did not make anything visible;
  the byte order above did. Keep it anyway, it is cheap and the GX2 docs require it.)
- **Cemu 2.6 cannot translate `MULHI_INT` or `MULHI_UINT`** ("Unable to compile shader <hash>" in its
  log, the dumped GLSL contains `Unsupported instruction;`), which CafeGLSL emits for an integer
  division by a constant. The batched sprite shaders compute `gl_VertexID / 6` and
  `(gl_VertexID + K) / 3`; `--emit-gx2` rewrites exactly these two forms to
  `int((float(x) + 0.5) / N.0)` (`MakeVertexIdDivisionsFloat`), exact for any vertex index a batch
  reaches. Other shaders may still contain integer divisions (check for "Unable to compile shader" in
  Cemu's log when a new screen is reached).
- The frame is serialised: `PresentFrame()` calls `GX2DrawDone()` after the swap and resets the uniform
  ring. GX2 does not rename buffers, so without it the engine would overwrite vertex and uniform data
  the GPU is still reading. Milestone 6 replaces this with fences and sectioned ring buffers.
- Logging: `Application.cpp` now passes the formatted entry to `OSReport()` as the format string (with
  `%` doubled), so Cemu's log shows the text instead of `"%s"`.

Open points for milestone 5:
- Render targets work for the menu's viewport target (tiled colour buffer sampled as a texture). Not
  exercised yet: lighting, blur, combine, water and the rescale shaders (they need the original game
  files in the test setup, see Testing), and `GetTexImage` readback.
- `ApplyTarget()` runs on every draw because the pipeline sets the viewport per draw, so
  `GX2SetColorBuffer` is called each time; only re-bind the colour buffer when the target changes.
- Attribute-based programs and `firstVertex`/`baseVertex` other than 0 are untested (see above).
- `Gx2Texture::TexSubImage2D` still rewrites texels in place; safe while every frame ends in
  `GX2DrawDone()`. (Milestone 6A: the host store is rewritten in place; the GPU copy moves to a new block when
  it is refreshed while still in use.)

## Milestone 5 results (2026-11-13)

Levels render through the GX2 backend in Cemu: tile layers, sprites, HUD, lighting with the blurred darkness
(`rescue/psych3`, 42 % light), the combine pass, and water with its surface line and the tinted, distorted scene
below it (`rescue/beach2`). `prince/castle1` matches the expected look. No "Unable to compile shader" in Cemu's
log for any of them, so all program variants these levels reach translate. The original game files for the test
setup are in `~/jazz2-game/jazz2` (retail copy) and were copied to Cemu's `sdcard/wiiu/apps/Jazz2/Source/`;
the first boot converts them into `Cache/`.

What was wrong and is fixed:
- **Mesh draws (tile layers, lighting) read zeros.** `RenderBuffersManager::Remap()` orphans the streaming
  buffers with `BufferData(size, nullptr)` right after the frame's draws were queued, and
  `Gx2BufferObject::BufferData()` zeroed the store on a null pointer. GX2 does not rename buffers and the GPU
  had not read the vertices and indices yet, so every indexed mesh was degenerate (sprites were unaffected:
  they take their data from the uniform ring, copied at draw time). `BufferData(size, nullptr)` now leaves the
  store alone (only a newly allocated block is zeroed), which is GL's orphaning contract. The next frame
  rewrites the store only after `PresentFrame()`'s `GX2DrawDone()`; milestone 6 replaces that with the
  sectioned ring (see milestone 4). This would have failed the same way on the console.
  How it was found, for similar cases: the GX2 trace showed correct `GX2SetAttribBuffer`/`GX2DrawIndexedEx2`
  calls and the data read back right at draw time, but drawing from a copy of the same bytes made the tiles
  appear. Cemu's buffer cache, index cache, culling, index format and the `GX2DrawIndexed*` variants were
  all ruled out along the way (the standalone spike draws indexed meshes with attributes fine).
- `ApplyTarget()` re-sent the colour buffer, viewport and scissor before every draw. It now remembers what was
  programmed (`Gx2Device::_applied`) and sends only what changed; a context-state restore (clears, scan-buffer
  copies, foreground) forgets it.

Rescale shaders (`prince/castle1` with each rescale mode through the test hook): all eight render in Cemu.
HQ2x was checked against a reference (the HQ2x formula in plain Python, applied to the pixel-perfect frame):
mean difference 0.6/255. Two Cemu 2.6 limitations broke three of them, both worked around in
`--emit-gx2` (the console runs either form; the workarounds keep the shaders correct there too):
- **Burst parameter exports.** CafeGLSL writes consecutive varyings with one burst export
  (`EXPORT_DONE PARAM 1-4 R1-4`). Cemu 2.6 translates only the first parameter of a burst
  (`LatteDecompilerEmitGLSL.cpp`, "export parameter"; fixed in Cemu main by "Latte: Support burst for vertex
  exports", 2026-10-02, not in a release yet), so the other varyings reach the pixel shader undefined. HQ2x
  came out ghosted, 3xBRZ flat brown. SABR also had a burst (it only looked right), and so do four batched
  background programs. `SplitParamExportBursts()` rewrites every burst into single exports and relocates the
  clauses and jump targets behind the grown CF program (padded to an even count for the 128-bit alignment of
  fetch clauses). Found by dumping Cemu's shaders: the vertex shader declared only two of HQ2x's five outputs.
- **`CEIL`.** Cemu (2.6 and main) has no translation for the R600 `CEIL` instruction ("Unsupported
  instruction;" in the dumped GLSL, then "Unable to compile shader"), which CafeGLSL emitted for CleanEdge's
  `ceil()`. The GX2 prelude now defines `ceil(x)` as `(-floor(-(x)))`, which is exact and compiles to `FLOOR`.
After both, none of the 97 program variants contains `CEIL` or `MULHI_*`, and every ALU opcode CafeGLSL emits is
one Cemu 2.6's translator handles (checked against its opcode list; `CNDGE`/`CNDGT` are its `CMOVGE`/`CMOVGT`).
`RHI_CAP_HEAVY_RESCALE_SHADERS` stays on. Whether they are fast enough is for milestone 6 on hardware.

Not done / still open:
- `Gx2Texture::GetTexImage()` on a render target still returns the host store. Nothing in the engine calls
  `Texture::SaveToMemory()` today, so it stays a stub.
- ImGui (debug overlay, not in release builds) is the remaining untested attribute program; see the byte-order
  note in milestone 4.
- The Software build is no reference for these effects (that tier leaves them out) and showed a black screen
  in `rescue/beach2` after 60 s in Cemu. A side-by-side check against a desktop build, or on the console, is
  still to do for the exact look of water and lighting.
- Test hook: `Sources/Main.cpp` and `Sources/Jazz2/LevelHandler.cpp` have local, uncommitted `WIIU_TEST_HOOK`
  blocks (Wii U only). The first reads `sd:/wiiu/apps/Jazz2/autolevel.txt`
  (`<episode>/<level> [rescale] [players] [walk] [noZoomOut]`, e.g. `rescue/psych3 2` or `prince/castle1 0 3 1`)
  and starts that level instead of the intro; 2-4 players start local co-op (Jazz, Spaz, Jazz, Spaz: the test data
  has no Lori). `walk` 1 makes every player run and jump, turning round every 900 frames. `noZoomOut` 1 turns off
  Zoom out. Delete the file to boot normally. Keep the hook out of commits (its patch is
  in `~/jazz2-wiiu-test/patches/autolevel-hook.patch`).

## Milestone 6 results, part A (2026-11-13)

The frame is no longer serialised: `PresentFrame()` submits the frame with `GX2Flush()` and returns after
`GX2WaitForFlip()`, without `GX2DrawDone()`, and `AllocateUniformData()` no longer calls it either. GX2 does not
rename buffers, so everything the CPU writes is now kept off memory a queued draw still reads:
- **Batch tokens as fences.** `Gx2Device` numbers its command batches (`GetUseToken()`, a 32-bit counter, never 0).
  `FlushBatch()` (at the present, or when a wait needs the open batch) calls `GX2Flush()` and keeps
  `GX2GetLastSubmittedTimeStamp()` for that batch in a 16-entry table; a token is retired once
  `GX2GetRetiredTimeStamp()` reaches its stamp (`IsUseRetired()`), and `WaitForUse()` flushes the batch if it is
  still open and then blocks in `GX2WaitTimeStamp()`. `InsertFence()` returns the open batch's token as the
  `FenceHandle`; `ClientWaitFence()` polls for a zero timeout and otherwise waits (GX2 has no timed wait;
  `GX2WaitTimeStamp()` gives up only on its own GPU timeout).
- **Streaming vertex and index buffers** use RenderBuffersManager's persistently mapped three-section ring, as on
  the RSX (`NCINE_HAS_PERSISTENT_MAPPING` + `NCINE_ALWAYS_PERSISTENT_MAPPING` for `WITH_RHI_GX2`). Unlike the RSX,
  the CPU cache is not coherent with the GPU, so `NCINE_PERSISTENT_MAPPING_NEEDS_FLUSH` makes `FlushUnmap()` flush
  the section's written range (`FlushMappedBufferRange()` -> `GX2Invalidate(CPU | ATTRIBUTE_BUFFER)`) before the
  frame's draws. The section fences are real here; `Application::Step()` waits for the next section after the
  present, as on the other platforms. The orphaning `BufferData(size, nullptr)` of milestone 5 is no longer
  called on these buffers.
- **Uniform ring:** the 4 MB ring is cut into eight 512 KB chunks. Leaving a chunk stamps it with the open batch;
  entering one waits for its stamp (logged as "The uniform ring caught up with the GPU", every 600th time).
  A chunk is the unit, not a frame, so a frame may use any number of them.
- **Textures and non-ring buffers:** every draw stamps the textures it samples and the vertex and index buffers it
  reads (`MarkUsed()`). A CPU write to one whose stamp has not retired either moves it to a fresh MEM2 block and
  retires the old one (what a driver does for an orphaned buffer: `BufferData()` on the same block, a whole-store
  `BufferSubData()`, a map with `InvalidateBuffer`, and the refresh of a texture's GPU copy from its host store),
  or waits for the stamp (partial `BufferSubData()`, other maps, `MapStreamingTexels()`, whose caller writes in
  place; Cinematics alternates two textures, so that wait has normally nothing to do). Persistent maps are never
  synchronised there. Uniform buffers are never stamped and no longer flushed: the device copies their bound
  ranges into its ring at draw time, so the GPU never reads them.
- **Retired blocks** (destroyed or reallocated resources) are freed once the batch that was open at retirement has
  retired, instead of two presents later.

Checked in Cemu (OpenGL), with a temporary trace (not committed) that logged the batch, the submitted and retired
stamps and the wait counts every 300 frames: main menu, the intro cinematic, `prince/castle1`, `rescue/psych3`
(lighting), `rescue/beach2` (water) and `prince/castle1 1` (rescale) render as in milestone 5, 2-4 screenshots per
run seconds apart, no flicker, missing tiles or garbage, no new warnings in the game log or "Unable to compile"
in Cemu's. The GPU had not retired the frame at any present (3000 of 3000 frames in flight), the section fence wait
ran once per frame and never blocked, and the uniform ring never caught up. GX2's timestamps in Cemu are OSTime
values, not counters, which the code does not depend on (it only compares them).
In these scenes nothing that a queued draw reads is rewritten (the camera stands still, and keyboard input did not
reach the game in the headless setup with a guessed Cemu controller profile), so a second temporary build treated every
resource drawn at least once as busy. The texture path then moved the GPU copy about 800 times during the intro
(each cinematic frame re-uploads its texture) and the intro and menu rendered correctly. The buffer rename and wait
paths are covered by the scrolling test below.

Only the console can say what this buys: Cemu's frame rate means nothing, and how far the CPU runs ahead depends
on the real GPU. Things to look at on hardware: whether `GX2WaitForFlip()` holds the CPU closer than one frame
behind the scan-out than needed (`GX2GetSwapStatus()` was the alternative noted in milestone 4), how often the
uniform ring warning appears, and frame time in the heaviest scenes (lighting plus a rescale shader).

## Cemu tests: scrolling and splitscreen (2026-11-13)

The local test hook (see Testing) now also makes every player run and jump, and can start local co-op with up to
four players.

**Scrolling.** `prince/castle1` with the player running back and forth renders without missing or garbled tiles.
The tile layers do not use custom buffers on GX2, though: `TILEMAP_CACHE_LAYER_WINDOW` (`Jazz2/Tiles/TileMap.h`)
is only defined for GL, D3D11, Vulkan, Metal and the GX/PICA/GS/GU tiers, so on GX2 every layer's quads go
through the streaming ring each frame, and in normal play nothing calls the buffer rename or wait paths of
`Gx2BufferObject::PrepareCpuWrite()`. To exercise them, a temporary build added `WITH_RHI_GX2` to that list:
the window cache then rewrote 48 KB tile buffers about 60 times a second while scrolling. Cemu's GPU had always
retired them by then, so two more temporary builds forced the busy case (every buffer drawn once counts as
busy): about 1400 renames, then, with the discard forced off, about 1100 waits, both with clean frames.
Whether to turn the window cache on for GX2 is a performance question for the console: it saves walking and
copying every layer's window each frame (an eighth of the main thread on desktop, a fifth on the PSP), and
the rename path gives it the same safety as a driver's orphaning.

**Splitscreen.** Two players (horizontal split, `prince/castle1` and `rescue/beach2`) render correctly,
including each viewport's HUD and the race-track minimap (`MpHUD::DrawMinimap()`, a `PolylineRenderer` line).
Three and four players run, but **with Zoom out (`PreferZoomOut`, on by default) the whole frame comes out with
wrong colours**: brown walls turn magenta, green turns blue. The average colour of a castle frame went from
(0.48, 0.25, 0.16) to (0.48, 0.16, 0.33): output green is roughly input blue, and output blue is something else.
- Cause, narrowed down: with three or more viewports and Zoom out, `LevelHandler::GetViewportSetup()`
  supersamples the scene by 2, and `UpscaleRenderPass` then resolves it to the screen through the
  `Antialiasing` program (bicubic, 16 `texture().rgb` taps). With Zoom out off, or with the resolve pass
  disabled in a temporary build, the colours are right, so the GX2 build of `Antialiasing` is wrong in Cemu.
  It is the only program that path adds, and no other test ran it.
- **Cause found (2026-11-14): a Cemu bug, not a CafeGLSL one.** The pixel shader is 558 dwords and 17 GPRs
  (`spikes/cafeglsl/t/Antialiasing.log`, `-v` disassembly; the committed program is byte-identical). Two
  independent checks:
  - An R600 ALU interpreter over that disassembly (`scripts/r600_emu_antialiasing.py <log> <seed>`; PV, PS,
    read-before-write within a group) gives exactly the GLSL reference for random textures, so CafeGLSL's code is
    correct.
  - Cemu's GLSL translation of the same program (dumped with the Debug > Dump > Shaders sequence under Testing,
    copied to `spikes/cafeglsl/t/Antialiasing.cemu_ps.txt`), run in Python
    (`scripts/cemu_glsl_emu_antialiasing.py <dump> <seed>`), gives the right red and wrong green and blue,
    which matches the screenshots.
- The mechanism: an OP3 instruction (`MULADD`) has no write mask, so CafeGLSL writes the results it only needs
  in PV/PS to the scratch register R123 (its disassembler prints that as `PV.c`/`PS.c`). In groups 15 and 37
  the `y` slot and the trans slot both write R123.y. The ISA allows this and defines it: R600 ISA section 4.9
  and R700 ISA section 4.9.2 recommend `DST_GPR = 123` to discard an OP3 result, and say that when ALU.Trans
  and a vector unit write the same GPR element, the trans result wins the GPR write and "the PV register
  reflects only the vector result". Cemu does not model that. `TrackGroupOutputPVPS()`
  (`LatteDecompilerEmitGLSL.cpp`, about line 327) points both PV.y and PS at the written GPR (R123.y), and
  the backups in `CreateGPRTemporaries()` are keyed by GPR element, so both reads get the trans result. The
  Metal emitter (`LatteDecompilerEmitMSL.cpp`) is a copy with the same bug, and there is no separate SPIR-V
  emitter. A fix would detect a vector unit and the trans unit writing the same (GPR, element) in one group,
  write the vector result to the existing `PV{n}` variable as well, right after its own statement, and map
  that unit with `SetLocationPSPVTemporary()` instead of `SetLocationGPR()`. Nothing like it is reported on
  Cemu's issue tracker.
- `python3 -I scripts/scan_cemu_pvps.py Sources/Shaders/Generated/Gx2GeneratedShaders.h` decodes every
  generated program and lists the groups where the trans slot writes the same GPR element as a vector slot
  and the next group reads that PV. 7 of 194 programs: `Antialiasing`, `Water`, `CombineWithWater`,
  `CombineWithWaterLow` (both with and without `DITHER`) and `ResizeCleanEdge`. In Cemu these are wrong too,
  just not as visibly as `Antialiasing` (the water scenes and CleanEdge looked right in milestone 5).
- **Confirmed on the console (2026-11-14):** three-player splitscreen in "Formerly a Prince", started from the
  menu with Zoom out on, has correct colours. A workaround in our
  tree would mean renaming the colliding R123 write to another dead register, which needs liveness
  analysis and risks the hardware for an emulator bug, so none was made. The proper fix is in Cemu (see
  above). A ready-to-submit issue with attachments is in
  `~/jazz2-wiiu-test/upstream/cemu-pvps-trans-collision/` (`REPORT.md`); submitting it is up to the owner.
- With the Software build, splitscreen showed a black screen in Cemu (as `rescue/beach2` did earlier); not
  looked into.

## Console benchmark (2026-11-14)

Both benchmark packages (see Testing) ran on the owner's console at 1080p with Zoom out on. The raw results are
`~/jazz2-wiiu-test/console-bench/bench-nocache.txt` and `bench-cache.txt`, the log is `Jazz2-bench.log`
there; no GPU timeouts or GX2 errors. Each phase lasted 15 s. "CPU" below is update + postUpdate + visit +
draw, with batching on unless noted. Everything else in a frame was spent in `GX2WaitForFlip()`: `gpuWait`
never showed up, so the CPU never had to wait for a fence, and `flipWait` holds both the idle time before
vsync and any GPU time beyond the CPU's. The benchmark cannot tell those two apart.

| Test | fps | CPU ms, no cache | CPU ms, cache |
|---|---|---|---|
| `prince/castle1`, 1 player | 60 | 2.2 | 0.6 |
| `rescue/beach2` (water), 1 player | 60 | 1.3 | 0.8 |
| `rescue/psych3` (dark), 1 player | 60 | 1.3 | 0.8 |
| `prince/castle1`, 2 players | 60 | 4.8 | 3.2 |
| `prince/castle1`, 4 players | 20 | 14.7 | 6.3 |
| `prince/castle1`, 4 players, no batching | 18.5-19 | 16.6 | 13.5 |

Rescale modes, `prince/castle1`, 1 player (the same with and without the cache): none and HQ2x 60 fps, SABR
30 fps, 3xBRZ 20 fps (also with 2 players), CleanEdge 8.6 fps (117 ms a frame). The CPU time is the same as
with no rescaling, so the rescale pass at 1920x1080 costs the GPU about 33 ms (3xBRZ) and 100 ms
(CleanEdge). The CRT and Monochrome modes were not tested.

Conclusions:
- **Tile-layer window cache: worth turning on.** It cuts visit by about 70% and draw by about 75% with one
  player, and halves the CPU time with four. No case got slower, and the frame rates did not change, because
  every test is either at 60 fps already or limited by the GPU.
- **Batching: keep it on.** On GX2 a batch is instanced sprites read from a uniform block, so this also
  covers the instancing path. It halves the draw calls (30 instead of 60 with one player, 109 instead of 304
  with four). With one or two players it changes nothing; with four, turning it off drops below 20 fps (up
  to 67 ms a frame).
- **Four players are GPU-bound.** The CPU needs 6-15 ms but frames take 50 ms. One player at 1080p
  fits in a frame, so raw fill rate is not the limit; the likely cost is the per-viewport passes (lighting,
  blur, combine), possibly with Zoom out enlarging each viewport's view. Finding out needs GPU timings
  (`GX2SampleBottomGPUCycle()` around the passes).
- **3xBRZ, SABR and CleanEdge are too slow at 1080p**, CleanEdge unplayably so. Options: hide them on GX2
  (as `RHI_CAP_HEAVY_RESCALE_SHADERS` does for other backends, though 3xBRZ is not under that flag), or run
  the rescale pass at 720p on the Wii U.

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
3. **Done.** Clear screen + present on TV and GamePad through the new device (no shaders).
4. **Done.** Textured sprite program: buffers, vertex format, one texture, uniforms. Main menu visible.
5. **Done.** `--emit-gx2` for all programs and variants; render targets (lighting, blur, combine, water); all
   rescale shaders.
6. A: **Done.** Fences, sectioned streaming and uniform rings, no `GX2DrawDone()` per frame.
   Scrolling and splitscreen checked in Cemu (see above); the `Antialiasing` colour bug with 3+
   players is Cemu's (confirmed on the console, 2026-11-14). The 1280x720 window cap is gone: GX2 builds
   output 1080p (commit `c90db7b8`, tested on the console). Measured on the console (see "Console benchmark"):
   the tile-layer window cache should be turned on for GX2, batching stays on. Still open: four-player GPU
   cost and the heavy rescale modes at 1080p.
7. Docs and CI: **done** (2026-11-13). `Docs/Consoles.dox` describes both backends (Software stays the
   default), `Docs/ShaderCompiler.dox` documents `--emit-gx2`, and `.github/workflows/wiiu.yml` builds both
   (`Jazz2_WiiU` and `Jazz2_WiiU_GX2`). The console tests are done (GX2 runs, 1080p, three-player
   splitscreen). `GX2` is the default since 2026-11-14 (`cmake/ncine_options.cmake`, `Docs/Consoles.dox`,
   `Docs/Building.dox`); CI now names the GX2 package `Jazz2_WiiU` and the Software one `Jazz2_WiiU_Software`.

## Testing

- Build: `devkitpro/devkitppc` Docker image, `-D CMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/WiiU.cmake`.
  `_build_wiiu/` is the Software build, `_build_wiiu_gx2/` the GX2 one. `~/jazz2-wiiu-test/scripts/build_gx2.sh`
  configures (first time) and builds `$B` (default `_build_wiiu_gx2`), keeps the log in
  `~/jazz2-wiiu-test/build.log` and prints only errors. A full GX2 build from scratch takes a few minutes.
- Cemu 2.6 runs headless under Xvfb in a container (`~/jazz2-wiiu-test/`: `cemu/` extracted AppImage
  in portable mode with a preseeded `settings.xml` that skips the first-run wizard, image
  `cemu-xvfb`, `run.sh` takes screenshots, `B=_build_wiiu_gx2 scripts/testrun.sh` repacks and runs that build, default `_build_wiiu`).
  **Cemu stops at every `tw` instruction**, including the conditional `tweq` GCC 16 emits for
  libstdc++'s `shared_ptr` overflow check. That shows up as a grey screen with the main thread parked
  in an OS wait. `scripts/patch_tw.py` replaces conditional traps with `nop` in a test copy of the ELF
  before `elf2rpl`/`wuhbtool`. Real hardware is not affected.
- Cemu's GDB stub (`Cemu --enable-gdbstub`, port 1337) works with `powerpc-eabi-gdb` from the
  devkitPPC image (`--network container:<cemu container>`); breakpoints with `commands` gave the
  backtrace that found the trap.
- Game log: `sd:/wiiu/apps/Jazz2/Jazz2.log` (Cemu: `portable/sdcard/...`), and every line also goes to
  `OSReport()`, shown in Cemu's `log.txt` when the log flags are set: `<logflag>4611686018427387904</logflag>`
  (bit 62, "Coreinit Logging") in `portable/settings.xml`. Cemu logs only the format string, which is
  why `Application.cpp` passes the formatted entry as the format. Bit 62 also traces every coreinit call
  (about 150k lines in 25 s), so grep for `OSReport`. All bits on (`18446744073709551615`) adds the GX2
  API calls with their arguments, useful to see what a frame submits. The preseeded settings have `0`.
- `Jazz2.log` loses its last lines when the test run kills Cemu. For a trace during play, set the coreinit log
  flag and call `OSReport()` with a preformatted string; the game's own `LOGI` lines stopped reaching Cemu's
  log after the level started in these runs.
- Cemu's frame rate says nothing about the console's. Performance has to be judged on hardware.
- Shader problems in Cemu: start Cemu without the game, turn on Debug > Dump > Shaders, then load the
  game with File > Load; Cemu writes its GLSL translation of every shader to `portable/dump/shaders/`.
  Without a window manager this is scripted with xdotool: image `cemu-xvfb-dbg` (`cemu-xvfb` plus
  `xdotool`, `glslang-tools`, `mesa-utils`) and `~/jazz2-wiiu-test/run_dbg.sh`, which runs `$XSTEPS`
  (`;`-separated xdotool commands, `sleep:N`, `shot:name`) after starting Cemu with `$CEMU_ARGS`
  (default `-g /t/jazz2.wuhb`). The sequence that worked:
  `CEMU_ARGS="" XSTEPS="sleep:5;mousemove 268 12;click 1;sleep:1;key Down;key Down;key Right;sleep:1;key Down;key Return;sleep:1;mousemove 18 12;click 1;sleep:1;key Down;key Return;sleep:2;key ctrl+l;sleep:1;type /t/jazz2.wuhb;key Return;sleep:15;shot:m1"`.
  `glslangValidator` on a dumped `.txt` shows why Cemu rejected it.
- `~/jazz2-wiiu-test/spikes/gx2-draw/` is a standalone program that draws `DefaultSprite` from the
  generated header with fixed uniforms (built like the other spikes, run with its `run.sh`); use it to
  try GX2 behaviour without the engine.
- ShaderCompiler for the host is built in the devkitPPC image into `~/jazz2-wiiu-test/sc_build`
  (`cmake -S Sources/Utilities/ShaderCompiler -B /b`); CafeGLSL's compiler is
  `~/jazz2-wiiu-test/spikes/cafeglsl/rel/cafeglsl-1.0.0-rc1-linux-x86_64/bin/glslcompiler`.
- Levels in Cemu: with the local test hook applied (`git apply ~/jazz2-wiiu-test/patches/autolevel-hook.patch`,
  never commit it), write `<episode>/<level> [rescale] [players] [walk] [noZoomOut]` to Cemu's `sdcard/wiiu/apps/Jazz2/autolevel.txt` and
  run `B=_build_wiiu_gx2 SHOTS="50" scripts/testrun.sh`; the game boots straight into that level. Remove the file
  to get the intro and menu again. Level names are the folders under `Cache/Episodes` (e.g. `prince/castle1`,
  `rescue/psych3` dark, `rescue/beach2` water). `python3 -I ~/jazz2-wiiu-test/scripts/scan_levels.py
  ~/jazz2-game/jazz2 208` lists the original levels with their start lighting and whether they set a water level.
- Console benchmark (2026-11-14): `~/jazz2-wiiu-test/patches/bench-hook.patch` (local only, never commit) is the
  autolevel hook plus a benchmark mode. If `bench.txt` (`<episode>/<level> <rescale> <players> [noZoomOut]` per
  line) is next to the game, it plays each test with every player running and jumping: 6 s warm-up, 15 s with
  batching on, 15 s with it off, then writes frame times, missed frames, phase times, draw calls and the GX2
  `flipWait`/`gpuWait` counters to `bench-<variant>.txt` and returns to the menu. The variant is `cache` when built
  with `-DWIIU_TEST_WINDOW_CACHE` in `CMAKE_CXX_FLAGS_RELEASE` (`_build_wiiu_gx2_cache/`, turns on
  `TILEMAP_CACHE_LAYER_WINDOW` for GX2), else `nocache`. Packages and the test list for the console are in
  `~/jazz2-wiiu-test/console-bench/`. In Cemu (meaningless for speed) the `cache` build dropped the visit phase from
  1.6 to 0.1 ms and lost the `gpuWait` stalls entirely; console results pending.
- Cemu's graphics API is OpenGL (`<api>0</api>` in `settings.xml`); Vulkan (`1`, lavapipe in the container)
  also works and behaved the same in milestone 5.
