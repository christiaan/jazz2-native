#if defined(WITH_RHI_GX2)

#include "Gx2Memory.h"

#include <coreinit/memdefaultheap.h>
#include <gx2/mem.h>

#if defined(WIIU_TEST_MEM1) // WIIU_TEST_HOOK (local only, do not commit)
#	include "../../../../Main.h"
#	include <cstring>
#	include <gx2/surface.h>
#	include <gx2r/surface.h>
#endif

namespace nCine::RHI::GX2::Gx2Memory
{
	namespace
	{
		std::uint32_t _allocatedBytes = 0;
#if defined(WIIU_TEST_MEM1)
		std::uint32_t _mem1Bytes = 0;
#endif
	}

#if defined(WIIU_TEST_MEM1)
	Block AllocSurfaceMem1(const GX2Surface& surface)
	{
		// GX2R allocates through the allocator SDL installed, which serves colour buffers from its MEM1 heap. A copy
		// of the surface is passed, so the caller's keeps no GX2R flags and is freed through Free() like any block.
		GX2Surface copy = surface;
		copy.image = nullptr;
		copy.mipmaps = nullptr;
		const GX2RResourceFlags flags = GX2RResourceFlags(GX2R_RESOURCE_BIND_COLOR_BUFFER | GX2R_RESOURCE_BIND_TEXTURE |
			GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ | GX2R_RESOURCE_USAGE_GPU_WRITE);
		Block block;
		if (!GX2RCreateSurface(&copy, flags) || copy.image == nullptr) {
			LOGW("MEM1: no room for a {}x{} surface ({} bytes, {} in MEM1 so far)", surface.width, surface.height, surface.imageSize, _mem1Bytes);
			return block;
		}
		block.Base = copy.image;
		block.Size = copy.imageSize;
		block.Mem1Flags = std::uint32_t(copy.resourceFlags);
		if (block.Mem1Flags == 0) {
			block.Mem1Flags = std::uint32_t(flags);
		}
		_mem1Bytes += block.Size;
		const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(block.Base);
		LOGI("MEM1: {}x{} surface at 0x{:.8x} ({} bytes, {} in MEM1){}", surface.width, surface.height, address, block.Size, _mem1Bytes,
			(address >= 0xF4000000u && address < 0xF6000000u) ? "" : " - NOT in MEM1");
		return block;
	}

	std::uint32_t GetMem1Bytes()
	{
		return _mem1Bytes;
	}
#endif

	Block Alloc(std::uint32_t size, std::uint32_t alignment)
	{
		Block block;
		if (size == 0) {
			return block;
		}
		if (alignment < 64) {
			alignment = 64;
		}
		block.Base = MEMAllocFromDefaultHeapEx(size, int(alignment));
		if (block.Base != nullptr) {
			block.Size = size;
			_allocatedBytes += size;
		}
		return block;
	}

	void Free(Block& block)
	{
#if defined(WIIU_TEST_MEM1)
		if (block.Base != nullptr && block.Mem1Flags != 0) {
			// GX2R frees through SDL's allocator, which only needs the image pointer and the flags
			GX2Surface surface;
			std::memset(&surface, 0, sizeof(surface));
			surface.image = block.Base;
			surface.imageSize = block.Size;
			surface.resourceFlags = GX2RResourceFlags(block.Mem1Flags);
			GX2RDestroySurfaceEx(&surface, GX2RResourceFlags(0));
			_mem1Bytes -= block.Size;
			block = Block{};
			return;
		}
#endif
		if (block.Base != nullptr) {
			MEMFreeToDefaultHeap(block.Base);
			_allocatedBytes -= block.Size;
		}
		block.Base = nullptr;
		block.Size = 0;
	}

	void FlushCpuWrites(const void* base, std::uint32_t size, std::uint32_t mode)
	{
		if (base != nullptr && size != 0) {
			GX2Invalidate(GX2InvalidateMode(mode | GX2_INVALIDATE_MODE_CPU), const_cast<void*>(base), size);
		}
	}

	std::uint32_t GetAllocatedBytes()
	{
		return _allocatedBytes;
	}
}

#endif
