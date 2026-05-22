#pragma once
#include <cstdint>

// These are 32-bit because Vulkan will force them to be
// 32-bit anyway, which can break indexing on the shader-side
namespace AssetDefs {
	typedef uint32_t DenseId;
	typedef uint32_t TextureSlot;
}