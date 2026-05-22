#pragma once
#include "utils/singleton.h"

#include <filesystem>
#include <functional>
#include <stack>

#include "utils/type_defs.h"

class Texture;

namespace AssetManagerGlobals {
	constexpr size_t gMaxTexCount = 100;
}

class AssetManager : public ISingleton<AssetManager>
{
public:
	std::function<void(const Texture&, AssetDefs::DenseId, AssetDefs::TextureSlot)> onTextureLoaded;

private:
	// Sized dynamically (as needed)
	std::unordered_map<std::string, Texture*> mTextures;
	std::unordered_map<uint64_t, AssetDefs::DenseId> mStableIdToDenseId;

	// Fixed size
	std::stack<AssetDefs::DenseId> mFreeDenseIds;
	std::stack<AssetDefs::TextureSlot> mFreeTextureSlots;

public:
	AssetManager();
	~AssetManager();

	const Texture& LoadTexture(const std::filesystem::path& _path);

	AssetDefs::DenseId GetDenseIdForTexture(const std::string& _texPathStr) const;
};