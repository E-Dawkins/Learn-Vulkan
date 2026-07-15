#pragma once
#include <cstdint>

namespace AssetDefs {
	// This can be whatever size honestly, but 64-bit scales
	// to practically an unlimited number of assets
	typedef uint64_t StableId;

	// These are 32-bit because Vulkan will force them to be
	// 32-bit anyway, which can break indexing on the shader-side
	typedef uint32_t DenseId;
	typedef uint32_t TextureSlot;
	typedef uint32_t CubemapSlot;
	typedef uint32_t MaterialSlot;
}