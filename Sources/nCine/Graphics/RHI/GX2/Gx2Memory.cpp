#if defined(WITH_RHI_GX2)

#include "Gx2Memory.h"

#include <coreinit/memdefaultheap.h>
#include <gx2/mem.h>

namespace nCine::RHI::GX2::Gx2Memory
{
	namespace
	{
		std::uint32_t _allocatedBytes = 0;
	}

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
