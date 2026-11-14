#pragma once

// WIIU_TEST_HOOK (local only, do not commit): switches for the console benchmark variants (Docs/WiiU-GX2-Plan.md,
// "Next console benchmark: variants"). Main.cpp sets them per test from the variant column of bench.txt; every
// switch is off outside the benchmark, so the game behaves as without the patch.

#include <cstdint>

namespace WiiUBenchFlag
{
	enum : std::uint32_t {
		SplitPresent = 1u << 0,		// B1: submit the draws, wait for the previous flip, then copy, swap and submit again
		WaitBeforeCopy = 1u << 1,	// B2: wait for the previous flip before the scan copies, one submit per frame
		FlushOnTarget = 1u << 2,	// B3: submit at every render-target switch that had draws
		DirectResolve = 1u << 3,	// C: no antialiasing subpass for supersampling alone, bicubic screen pass instead
		LinearResolve = 1u << 4,	// D: as C, with a linear screen pass
		NoSupersample = 1u << 5,	// E: Supersample = 1 with Zoom out
		DrcOnce = 1u << 6,			// G: GamePad copy only in the first two frames of a measurement
		DrcEveryOther = 1u << 7,	// G2: GamePad copy in two frames of every four
	};
}

/** @brief Active variant switches (`WiiUBenchFlag`) */
extern std::uint32_t WiiUBenchFlags;
/** @brief Frames presented since the measurement started (the GamePad variants count from it) */
extern std::uint32_t WiiUBenchFrame;
/** @brief Times the uniform ring had to wait for the GPU / `RetireBlock()` fell back to `Finish()` */
extern std::uint32_t WiiUBenchRingWaits, WiiUBenchRetireFinishes;
/** @brief Set around the creation of the targets the MEM1 build puts in MEM1 (`WIIU_TEST_MEM1`) */
extern bool WiiUBenchMem1Hint;
/** @brief While the benchmark overrides preferences: the owner's own values, which `PreferencesCache::Save()` writes instead */
extern bool WiiUBenchPrefsOverridden;
extern std::uint8_t WiiUBenchSavedRescale, WiiUBenchSavedLighting;
extern bool WiiUBenchSavedZoomOut;
