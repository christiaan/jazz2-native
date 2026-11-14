// Spike: draw DefaultSprite from Gx2GeneratedShaders.h with fixed uniforms on SDL's GX2 setup
#include <SDL2/SDL.h>
#include <coreinit/debug.h>
#include <coreinit/memdefaultheap.h>
#include <gx2/context.h>
#include <gx2/clear.h>
#include <gx2/display.h>
#include <gx2/draw.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/state.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <gx2/texture.h>
#include <gx2/utils.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include "Gx2GeneratedShaders.h"
#ifndef BUFMODE
#define BUFMODE 0
#endif
#ifndef DRAWMODE
#define DRAWMODE 0
#endif
using namespace nCine::RHI::GX2;
static char logbuf[512];
#define LOG(...) do { snprintf(logbuf, sizeof(logbuf), __VA_ARGS__); OSReport(logbuf); } while (0)

static void* galloc(uint32_t size, uint32_t align) { void* p = MEMAllocFromDefaultHeapEx(size, align); memset(p, 0, size); return p; }

template<typename T> static void fill(T& sh, const GeneratedGx2Stage& st)
{
	memset(&sh, 0, sizeof(sh));
	memcpy(&sh.regs, st.Registers, std::min(sizeof(sh.regs), size_t(st.RegisterCount) * 4));
	sh.size = st.ProgramSize;
	sh.program = galloc(st.ProgramSize, GX2_SHADER_PROGRAM_ALIGNMENT);
	memcpy(sh.program, st.Program, st.ProgramSize);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, sh.program, sh.size);
	sh.mode = GX2_SHADER_MODE_UNIFORM_BLOCK;
	sh.uniformBlockCount = st.UniformBlockCount; sh.uniformBlocks = (GX2UniformBlock*)st.UniformBlocks;
	sh.uniformVarCount = st.UniformVarCount; sh.uniformVars = (GX2UniformVar*)st.UniformVars;
	sh.loopVarCount = st.LoopVarCount; sh.loopVars = (GX2LoopVar*)st.LoopVars;
	sh.samplerVarCount = st.SamplerVarCount; sh.samplerVars = (GX2SamplerVar*)st.SamplerVars;
}
static void fillVs(GX2VertexShader& sh, const GeneratedGx2Stage& st) { fill(sh, st); sh.attribVarCount = st.AttribVarCount; sh.attribVars = (GX2AttribVar*)st.AttribVars; }

int main(int argc, char** argv)
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
	SDL_Window* win = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, 0);
	GX2ContextState* ctx = (GX2ContextState*)memalign(GX2_CONTEXT_STATE_ALIGNMENT, sizeof(GX2ContextState));
	GX2SetupContextStateEx(ctx, TRUE);
	GX2SetContextState(ctx);
	GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
	GX2SetSwapInterval(1);

	GX2ColorBuffer cb; memset(&cb, 0, sizeof(cb));
	cb.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D; cb.surface.width = 1280; cb.surface.height = 720; cb.surface.depth = 1;
	cb.surface.mipLevels = 1; cb.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8; cb.surface.aa = GX2_AA_MODE1X;
	cb.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV; cb.surface.tileMode = GX2_TILE_MODE_DEFAULT; cb.viewNumSlices = 1;
	GX2CalcSurfaceSizeAndAlignment(&cb.surface);
	cb.surface.image = galloc(cb.surface.imageSize, cb.surface.alignment);
	GX2InitColorBufferRegs(&cb);

	const GeneratedGx2Shader* gs = FindGeneratedGx2Shader("DefaultSprite", "");
	LOG("SPIKE: shader %p\n", gs);
	GX2VertexShader vs; GX2PixelShader ps;
	fill(vs, gs->Vertex); fill(ps, gs->Pixel);

	GX2FetchShader fs; memset(&fs, 0, sizeof(fs));
	uint32_t fsSize = GX2CalcFetchShaderSizeEx(0, GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
	void* fsProg = galloc(fsSize, GX2_SHADER_PROGRAM_ALIGNMENT);
	GX2InitFetchShaderEx(&fs, (uint8_t*)fsProg, 0, nullptr, GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, fsProg, fsSize);
	LOG("SPIKE: fetch shader size %u\n", fsSize);

	// 4x4 white texture
	GX2Texture tex; memset(&tex, 0, sizeof(tex));
	tex.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D; tex.surface.width = 4; tex.surface.height = 4; tex.surface.depth = 1;
	tex.surface.mipLevels = 1; tex.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8; tex.surface.aa = GX2_AA_MODE1X;
	tex.surface.use = GX2_SURFACE_USE_TEXTURE; tex.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
	tex.viewNumSlices = 1; tex.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
	GX2CalcSurfaceSizeAndAlignment(&tex.surface);
	tex.surface.image = galloc(tex.surface.imageSize, tex.surface.alignment);
	memset(tex.surface.image, 0xff, tex.surface.imageSize);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, tex.surface.image, tex.surface.imageSize);
	GX2InitTextureRegs(&tex);
	GX2Sampler smp; GX2InitSampler(&smp, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);

	// Uniforms: loose block = proj, view (identity); InstanceBlock = model identity, color, texRect, spriteSize
	float* loose = (float*)galloc(256, 256);
	float* inst = (float*)galloc(256, 256);
	for (int i = 0; i < 4; i++) { loose[i * 5] = 1.0f; loose[16 + i * 5] = 1.0f; inst[i * 5] = 1.0f; }
	float rest[] = { 1, 1, 0, 1,   1, 0, 1, 0,   1, 1, 0, 0 };	// color (yellow), texRect, spriteSize
	memcpy(inst + 16, rest, sizeof(rest));
	inst[12] = -1.0f;	// quad A: model translation x = -1 (left half)
	float* looseB = (float*)galloc(256, 256);
	float* instB = (float*)galloc(256, 256);
	memcpy(looseB, loose, 256); memcpy(instB, inst, 256);
	instB[12] = 0.0f; instB[13] = -1.0f;	// quad B: bottom right
	for (int i = 0; i < 64; i++) { ((uint32_t*)looseB)[i] = __builtin_bswap32(((uint32_t*)looseB)[i]); ((uint32_t*)instB)[i] = __builtin_bswap32(((uint32_t*)instB)[i]); }
	GX2Invalidate((GX2InvalidateMode)(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), loose, 256);
	GX2Invalidate((GX2InvalidateMode)(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), inst, 256);
	GX2Invalidate((GX2InvalidateMode)(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), looseB, 256);
	GX2Invalidate((GX2InvalidateMode)(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), instB, 256);

	for (uint32_t i = 0; i < vs.uniformBlockCount; i++) LOG("SPIKE: vs block %s loc %u size %u\n", vs.uniformBlocks[i].name, vs.uniformBlocks[i].offset, vs.uniformBlocks[i].size);
	for (uint32_t i = 0; i < ps.samplerVarCount; i++) LOG("SPIKE: ps sampler %s loc %u\n", ps.samplerVars[i].name, ps.samplerVars[i].location);

	uint16_t* idx = (uint16_t*)galloc(256, 256);
	idx[0] = 0; idx[1] = 1; idx[2] = 2; idx[3] = 3;
	GX2Invalidate((GX2InvalidateMode)(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_ATTRIBUTE_BUFFER), idx, 256);
	const GeneratedGx2Shader* gt = FindGeneratedGx2Shader("TileMapMesh", "");
	GX2VertexShader tvs; GX2PixelShader tps; fillVs(tvs, gt->Vertex); fill(tps, gt->Pixel);
	for (uint32_t i = 0; i < tvs.attribVarCount; i++) LOG("SPIKE: attrib %s loc %u\n", tvs.attribVars[i].name, tvs.attribVars[i].location);
	auto locOf = [&](const char* n) { for (uint32_t i = 0; i < tvs.attribVarCount; i++) if (!strcmp(tvs.attribVars[i].name, n)) return tvs.attribVars[i].location; return 0u; };
	GX2AttribStream st[3]; memset(st, 0, sizeof(st));
	const char* names[3] = { "aPosition", "aTexCoords", "aColor" };
	GX2AttribFormat fmts[3] = { GX2_ATTRIB_FORMAT_FLOAT_32_32, GX2_ATTRIB_FORMAT_FLOAT_32_32, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32 };
	uint32_t offs[3] = { 0, 8, 16 };
	for (int i = 0; i < 3; i++) {
		st[i].location = locOf(names[i]); st[i].buffer = BUFMODE ? 0 : i; st[i].offset = offs[i]; st[i].format = fmts[i];
		st[i].type = GX2_ATTRIB_INDEX_PER_VERTEX; st[i].aluDivisor = 0;
		st[i].mask = (i == 2) ? GX2_COMP_MAP(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W) : GX2_COMP_MAP(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
		st[i].endianSwap = GX2_ENDIAN_SWAP_DEFAULT;
	}
	GX2FetchShader tfs; memset(&tfs, 0, sizeof(tfs));
	uint32_t tfsSize = GX2CalcFetchShaderSizeEx(3, GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
	void* tfsProg = galloc(tfsSize, GX2_SHADER_PROGRAM_ALIGNMENT);
	GX2InitFetchShaderEx(&tfs, (uint8_t*)tfsProg, 3, st, GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, tfsProg, tfsSize);
	// 8 vertices: 4 junk + quad (x -0.9..-0.1, y 0.1..0.9) at base 4
	float* vb = (float*)galloc(8 * 32, 256);
	float quad[4][8] = { { -0.9f, 0.1f, 0, 0, 0, 1, 0, 1 }, { -0.1f, 0.1f, 1, 0, 0, 1, 0, 1 }, { -0.9f, 0.9f, 0, 1, 0, 1, 0, 1 }, { -0.1f, 0.9f, 1, 1, 0, 1, 0, 1 } };
	memcpy(vb + 4 * 8, quad, sizeof(quad));
	GX2Invalidate((GX2InvalidateMode)(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_ATTRIBUTE_BUFFER), vb, 8 * 32);
	uint16_t* tidx = (uint16_t*)galloc(256, 256);
	tidx[0] = 0; tidx[1] = 1; tidx[2] = 2; tidx[3] = 2; tidx[4] = 1; tidx[5] = 3;
	GX2Invalidate((GX2InvalidateMode)(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_ATTRIBUTE_BUFFER), tidx, 256);
	float* tinst = (float*)galloc(256, 256);
	for (int i = 0; i < 4; i++) tinst[i * 5] = 1.0f;
	tinst[16] = 1; tinst[17] = 1; tinst[18] = 1; tinst[19] = 1;
	for (int i = 0; i < 64; i++) ((uint32_t*)tinst)[i] = __builtin_bswap32(((uint32_t*)tinst)[i]);
	GX2Invalidate((GX2InvalidateMode)(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), tinst, 256);
	int frame = 0;
	while (frame < 100000) {
		SDL_Event e;
		bool quit = false;
		while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) quit = true; }
		if (quit) break;

		GX2ClearColor(&cb, 0.0f, 0.0f, 0.5f, 1.0f);
		GX2SetContextState(ctx);
		GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
		GX2SetColorBuffer(&cb, GX2_RENDER_TARGET_0);
		GX2SetViewport(0, 0, 1280, 720, 0.0f, 1.0f);
		GX2SetScissor(0, 0, 1280, 720);
		GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_LEQUAL);
		GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
		GX2SetTargetChannelMasks(GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA,
			GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA);
		GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
		GX2SetFetchShader(&fs);
		GX2SetVertexShader(&vs);
		GX2SetPixelShader(&ps);
		GX2SetVertexUniformBlock(0, 128, loose);
		GX2SetVertexUniformBlock(1, 112, inst);
		GX2SetPixelTexture(&tex, 0);
		GX2SetPixelSampler(&smp, 0);
		GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, 0, 1);
		GX2SetVertexUniformBlock(0, 128, looseB);
		GX2SetVertexUniformBlock(1, 112, instB);
		GX2DrawIndexedEx2(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, GX2_INDEX_TYPE_U16, idx, 0, 1, 0);

		GX2SetFetchShader(&tfs);
		GX2SetVertexShader(&tvs);
		GX2SetPixelShader(&tps);
		GX2SetVertexUniformBlock(0, 128, looseB);
		GX2SetVertexUniformBlock(1, 112, tinst);
		GX2SetPixelTexture(&tex, 0);
		GX2SetPixelSampler(&smp, 0);
		for (int i = 0; i < 3; i++) GX2SetAttribBuffer(BUFMODE ? 0 : i, 8 * 32, 32, vb);
		if (DRAWMODE == 0) GX2DrawIndexedEx2(GX2_PRIMITIVE_MODE_TRIANGLES, 6, GX2_INDEX_TYPE_U16, tidx, 4, 1, 0);
		else GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, 4, 1);
		GX2CopyColorBufferToScanBuffer(&cb, GX2_SCAN_TARGET_TV);
		GX2CopyColorBufferToScanBuffer(&cb, GX2_SCAN_TARGET_DRC);
		GX2SetContextState(ctx);
		GX2SwapScanBuffers();
		GX2Flush();
		GX2DrawDone();
		GX2WaitForFlip();
		if (frame == 0) { GX2SetTVEnable(TRUE); GX2SetDRCEnable(TRUE); }
		frame++;
	}
	SDL_Quit();
	return 0;
}
