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
	std::function<void(AssetDefs::TextureSlot)> onTextureUnloaded;

private:
	// Sized dynamically (as needed)
	std::unordered_map<std::string, Texture*> mTextures;
	std::unordered_map<uint64_t, AssetDefs::DenseId> mStableIdToDenseId;
	std::unordered_map<AssetDefs::DenseId, AssetDefs::TextureSlot> mDenseIdToTextureSlot;

	// Fixed size
	std::stack<AssetDefs::DenseId> mFreeDenseIds;
	std::stack<AssetDefs::TextureSlot> mFreeTextureSlots;

public:
	AssetManager();
	~AssetManager();

	const Texture& LoadTexture(const std::filesystem::path& _path);
	void UnloadTexture(const std::string& _texPathStr);

	AssetDefs::DenseId GetDenseIdForTexture(const std::string& _texPathStr) const;
	bool IsTextureLoaded(const std::string& _texPathStr) const;
};