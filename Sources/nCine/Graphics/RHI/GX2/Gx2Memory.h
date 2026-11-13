#pragma once

#include <cstddef>
#include <cstdint>

namespace nCine::RHI::GX2
{
	/**
		@brief GPU-visible memory of the GX2 backend

		The Wii U's GPU (the R700-class "Latte") reads system memory directly, so unlike the PlayStation 3's
		RSX there is no address window and no offset translation: a block is a plain CPU pointer that is also
		the address the GPU fetches from. What the CPU has to do is keep its caches out of the way. The
		Espresso's data cache is not coherent with the GPU, so every CPU write the GPU is meant to see has to
		be flushed with @ref FlushCpuWrites() (`GX2Invalidate()` with the matching mode, which also drops the
		GPU's own cache lines for that range) before the draw that reads it is submitted.

		<b>Everything is in MEM2 for now.</b> wiiu-sdl2 takes all of MEM1 and the foreground heap when its
		video driver starts, and it frees that MEM1 heap whenever the application is sent to the background
		(HOME menu), so anything the backend kept there would have to be destroyed and recreated around each
		trip. MEM2 survives the background, so every block - buffers, textures, the screen colour and depth
		buffers - comes from the default heap (`MEMAllocFromDefaultHeapEx()`). Moving the render targets to
		MEM1 is a later optimisation that needs measuring on hardware first.
	*/
	namespace Gx2Memory
	{
		/** @brief One allocation; `Base` is both the CPU and the GPU address */
		struct Block
		{
			void* Base = nullptr;
			std::uint32_t Size = 0;

			inline bool IsValid() const {
				return (Base != nullptr);
			}
		};

		/**
			@brief Allocates GPU-visible memory from MEM2

			@param size       Requested size in bytes
			@param alignment  Required alignment. Vertex and index buffers need 64 bytes
			                  (`GX2_VERTEX_BUFFER_ALIGNMENT`), uniform blocks 256 (`GX2_UNIFORM_BLOCK_ALIGNMENT`)
			                  and surfaces whatever `GX2CalcSurfaceSizeAndAlignment()` reports
		*/
		Block Alloc(std::uint32_t size, std::uint32_t alignment);
		/** @brief Releases a block and resets it; the caller must know the GPU is done with it */
		void Free(Block& block);

		/** @brief Flushes CPU writes to a range out of the data cache and invalidates the GPU caches given by @p mode */
		void FlushCpuWrites(const void* base, std::uint32_t size, std::uint32_t mode);

		/** @brief Bytes currently allocated through @ref Alloc() */
		std::uint32_t GetAllocatedBytes();
	}
}
