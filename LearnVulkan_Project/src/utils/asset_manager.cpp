#include "pch.h"
#include "utils/asset_manager.h"

#include "renderer/material.h"
#include "renderer/texture.h"

#define DEFINE_ASSET_HELPERS(TYPE, ASSET_MAP) \
	AssetDefs::DenseId AssetManager::GetDenseIdFor ## TYPE ## (const std::string& _pathStr) const { \
		assert(Is ## TYPE ## Loaded(_pathStr)); \
		TYPE* asset = ASSET_MAP.at(_pathStr); \
		assert(asset); \
		return GetDenseIdForStableId(asset->GetStableId()); \
	} \
	bool AssetManager::Is ## TYPE ## Loaded(const std::string& _pathStr) const { \
		return ASSET_MAP.contains(_pathStr); \
	}

AssetManager::~AssetManager() {
	for (auto& [path, tex] : mTextures) {
		delete tex;
	}
}

std::string AssetManager::StripFirstFolder(const std::filesystem::path& _path) {
	std::filesystem::path relativePath;
	for (std::filesystem::path::iterator itr = (++_path.begin()); itr != _path.end(); ++itr) {
		relativePath /= *itr;
	}

	return relativePath.string();
}

AssetDefs::DenseId AssetManager::GetDenseIdForStableId(AssetDefs::StableId _stableId) const {
	assert(mStableIdToDenseId.contains(_stableId));
	return mStableIdToDenseId.at(_stableId);
}

const Texture& AssetManager::LoadTexture(const std::filesystem::path& _path) {
	const std::string assetName = StripFirstFolder(_path);

	// Store texture in asset map
	mTextures[assetName] = new Texture(_path);
	const Texture& loadedTex = *mTextures[assetName];

	// Store stable -> dense id mapping
	AssetDefs::DenseId denseId = mTextureSlotAllocator.AllocateId();
	mStableIdToDenseId[loadedTex.GetStableId()] = denseId;

	// Finally, call onTextureLoaded callback
	if (onTextureLoaded) {
		onTextureLoaded(loadedTex, denseId, mTextureSlotAllocator.GetSlotForId(denseId));
	}

	return loadedTex;
}

void AssetManager::UnloadTexture(const std::string& _pathStr) {
	if (!mTextures.contains(_pathStr)) {
		return; // no texture found with the passed in path string
	}

	// DO NOT ALLOW UNLOADING THE DEFAULT TEXTURE, IT IS VITAL
	if (_pathStr == "textures\\default_texture.png") {
		std::cerr << "Someone tried to unload 'default_texture.png' ... naughty!\n";
		return;
	}

	Texture* texToUnload = mTextures[_pathStr];
	if (!texToUnload) {
		// Texture is already null, just remove it from the map
		mTextures.erase(_pathStr);
		return;
	}

	// Retrieve all stored ids
	AssetDefs::StableId stableId = texToUnload->GetStableId();
	AssetDefs::DenseId denseId = mStableIdToDenseId[stableId];

	// Free mappings
	mTextureSlotAllocator.FreeId(denseId);
	mStableIdToDenseId.erase(stableId);

	// Remove texture from asset map
	mTextures.erase(_pathStr);
	delete texToUnload;

	// Finally, call onTextureUnloaded callback
	if (onTextureUnloaded) {
		onTextureUnloaded(denseId);
	}
}

const Material& AssetManager::LoadMaterial(const std::filesystem::path& _path) {
	const std::string assetName = StripFirstFolder(_path);

	// TODO: actually load material from file

	const Material& loadedMat = *mMaterials[assetName];
	return loadedMat;
}

void AssetManager::UnloadMaterial(const std::string& /*_pathStr*/) {
	// TODO: unload material
}

DEFINE_ASSET_HELPERS(Texture, mTextures)
DEFINE_ASSET_HELPERS(Material, mMaterials)
