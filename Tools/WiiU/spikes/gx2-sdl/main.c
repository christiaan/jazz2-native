// Spike: SDL2 video stays initialised (ProcUI, GX2Init, scan buffers), no SDL_Renderer;
// the app renders with its own GX2ContextState into a GX2R colour buffer and presents itself.
#include <SDL2/SDL.h>
#include <coreinit/debug.h>
#include <coreinit/time.h>
#include <gx2/context.h>
#include <gx2/clear.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/state.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <gx2r/surface.h>
#include <gx2r/buffer.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <sysapp/launch.h>
static char logbuf[256];
#define LOG(...) do { snprintf(logbuf, sizeof(logbuf), __VA_ARGS__); OSReport(logbuf); } while (0)

static GX2ContextState* ctx;
static GX2ColorBuffer cb;
static int haveCb;

static void createTargets(void)
{
	memset(&cb, 0, sizeof(cb));
	cb.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	cb.surface.width = 1280;
	cb.surface.height = 720;
	cb.surface.depth = 1;
	cb.surface.mipLevels = 1;
	cb.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	cb.surface.aa = GX2_AA_MODE1X;
	cb.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
	cb.surface.tileMode = GX2_TILE_MODE_DEFAULT;
	cb.viewNumSlices = 1;
	BOOL ok = GX2RCreateSurface(&cb.surface, GX2R_RESOURCE_BIND_COLOR_BUFFER | GX2R_RESOURCE_BIND_TEXTURE |
		GX2R_RESOURCE_USAGE_GPU_READ | GX2R_RESOURCE_USAGE_GPU_WRITE);
	GX2InitColorBufferRegs(&cb);
	haveCb = ok;
	LOG("SPIKE: colour buffer ok=%d image=%p size=0x%x\n", ok, cb.surface.image, cb.surface.imageSize);
	// A MEM2 buffer through GX2R as well, to see where non-target resources land
	GX2RBuffer vb = {0};
	vb.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ;
	vb.elemSize = 16; vb.elemCount = 64;
	ok = GX2RCreateBuffer(&vb);
	LOG("SPIKE: vertex buffer ok=%d buffer=%p\n", ok, vb.buffer);
	GX2RDestroyBufferEx(&vb, 0);
}

static void destroyTargets(void)
{
	if (haveCb) {
		GX2DrawDone();
		GX2RDestroySurfaceEx(&cb.surface, 0);
		haveCb = 0;
		LOG("SPIKE: colour buffer destroyed\n");
	}
}

int main(int argc, char** argv)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
		LOG("SPIKE: SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window* win = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, 0);
	LOG("SPIKE: window=%p renderer=%p swapInterval=%u\n", win, SDL_GetRenderer(win), GX2GetSwapInterval());

	ctx = (GX2ContextState*)memalign(GX2_CONTEXT_STATE_ALIGNMENT, sizeof(GX2ContextState));
	GX2SetupContextStateEx(ctx, TRUE);
	GX2SetContextState(ctx);
	GX2SetSwapInterval(1);
	createTargets();

	int running = 1, frame = 0, bg = 0;
	Uint32 start = SDL_GetTicks();
	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
				case SDL_QUIT: LOG("SPIKE: SDL_QUIT\n"); running = 0; break;
				case SDL_APP_WILLENTERBACKGROUND: LOG("SPIKE: WILLENTERBACKGROUND\n"); destroyTargets(); bg = 1; break;
				case SDL_APP_DIDENTERBACKGROUND: LOG("SPIKE: DIDENTERBACKGROUND\n"); break;
				case SDL_APP_WILLENTERFOREGROUND: LOG("SPIKE: WILLENTERFOREGROUND\n"); break;
				case SDL_APP_DIDENTERFOREGROUND: LOG("SPIKE: DIDENTERFOREGROUND\n"); if (!haveCb) createTargets(); bg = 0; break;
				case SDL_CONTROLLERBUTTONDOWN: LOG("SPIKE: button %d\n", e.cbutton.button); break;
				case SDL_FINGERDOWN: LOG("SPIKE: touch %f %f\n", e.tfinger.x, e.tfinger.y); break;
				default: break;
			}
		}
		if (!running) break;
		if (bg || !haveCb) continue;

		float t = (frame % 120) / 120.0f;
		GX2ClearColor(&cb, t, 0.2f, 1.0f - t, 1.0f);
		GX2SetContextState(ctx);
		GX2CopyColorBufferToScanBuffer(&cb, GX2_SCAN_TARGET_TV);
		GX2CopyColorBufferToScanBuffer(&cb, GX2_SCAN_TARGET_DRC);
		GX2SwapScanBuffers();
		GX2Flush();
		GX2WaitForFlip();
		if (frame == 0) {
			GX2SetTVEnable(TRUE);
			GX2SetDRCEnable(TRUE);
		}
		frame++;
		if (frame == 400) {
			LOG("SPIKE: calling SYSLaunchMenu\n");
			SYSLaunchMenu();
		}
		if (frame % 300 == 0) {
			LOG("SPIKE: %d frames, %.1f fps\n", frame, frame * 1000.0f / (SDL_GetTicks() - start));
		}
	}
	destroyTargets();
	free(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();
	LOG("SPIKE: clean exit\n");
	return 0;
}
