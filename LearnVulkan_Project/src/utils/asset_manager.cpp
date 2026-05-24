#include "pch.h"
#include "utils/asset_manager.h"

#include "renderer/texture.h"

AssetManager::~AssetManager() {
	for (auto& [path, tex] : mTextures) {
		delete tex;
	}
}

const Texture& AssetManager::LoadTexture(const std::filesystem::path& _path) {
	// Strip first folder, should be 'assets', for ease-of-use when retrieving Texture*
	std::filesystem::path relativePath;
	for (std::filesystem::path::iterator itr = (++_path.begin()); itr != _path.end(); ++itr) {
		relativePath /= *itr;
	}

	// Store texture in asset map
	mTextures[relativePath.string()] = new Texture(_path);
	const Texture& loadedTex = *mTextures[relativePath.string()];

	// Store stable -> dense id mapping
	AssetDefs::DenseId denseId = mTextureSlotAllocator.AllocateId();
	mStableIdToDenseId[loadedTex.GetStableId()] = denseId;

	// Finally, call onTextureLoaded callback
	if (onTextureLoaded) {
		onTextureLoaded(loadedTex, denseId, mTextureSlotAllocator.GetSlotForId(denseId));
	}

	return loadedTex;
}

void AssetManager::UnloadTexture(const std::string& _texPathStr) {
	if (!mTextures.contains(_texPathStr)) {
		return; // no texture found with the passed in path string
	}

	// DO NOT ALLOW UNLOADING THE DEFAULT TEXTURE, IT IS VITAL
	if (_texPathStr == "textures\\default_texture.png") {
		std::cerr << "Someone tried to unload 'default_texture.png' ... naughty!\n";
		return;
	}

	Texture* texToUnload = mTextures[_texPathStr];
	if (!texToUnload) {
		// Texture is already null, just remove it from the map
		mTextures.erase(_texPathStr);
		return;
	}

	// Retrieve all stored ids
	AssetDefs::StableId stableId = texToUnload->GetStableId();
	AssetDefs::DenseId denseId = mStableIdToDenseId[stableId];

	// Free mappings
	mTextureSlotAllocator.FreeId(denseId);
	mStableIdToDenseId.erase(stableId);

	// Remove texture from asset map
	mTextures.erase(_texPathStr);
	delete texToUnload;

	// Finally, call onTextureUnloaded callback
	if (onTextureUnloaded) {
		onTextureUnloaded(denseId);
	}
}

AssetDefs::DenseId AssetManager::GetDenseIdForTexture(const std::string& _texPathStr) const {
	assert(mTextures.contains(_texPathStr));

	Texture* tex = mTextures.at(_texPathStr);
	assert(tex);

	return mStableIdToDenseId.at(tex->GetStableId());
}

bool AssetManager::IsTextureLoaded(const std::string& _texPathStr) const {
	return mTextures.contains(_texPathStr);
}
