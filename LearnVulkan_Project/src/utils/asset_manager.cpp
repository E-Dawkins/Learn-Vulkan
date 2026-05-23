#include "pch.h"
#include "utils/asset_manager.h"

#include "renderer/texture.h"

AssetManager::AssetManager() {
	for (uint16_t i = 0; i < AssetManagerGlobals::gMaxTexCount; i++) {
		mFreeDenseIds.push(AssetManagerGlobals::gMaxTexCount - i - 1);
		mFreeTextureSlots.push(AssetManagerGlobals::gMaxTexCount - i - 1);
	}
}

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
	assert(!mFreeDenseIds.empty());
	AssetDefs::DenseId denseId = mFreeDenseIds.top();
	mStableIdToDenseId[loadedTex.GetStableId()] = denseId;
	mFreeDenseIds.pop();

	// Grab next available texture slot
	assert(!mFreeTextureSlots.empty());
	AssetDefs::TextureSlot textureSlot = mFreeTextureSlots.top();
	mDenseIdToTextureSlot[denseId] = textureSlot;
	mFreeTextureSlots.pop();

	// Finally, call onTextureLoaded callback
	if (onTextureLoaded) {
		onTextureLoaded(loadedTex, denseId, textureSlot);
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
	uint64_t stableId = texToUnload->GetStableId();
	AssetDefs::DenseId denseId = mStableIdToDenseId[stableId];
	AssetDefs::TextureSlot textureSlot = mDenseIdToTextureSlot[denseId];

	// Free texture slot
	mDenseIdToTextureSlot.erase(denseId);
	mFreeTextureSlots.push(textureSlot);

	// Free dense id
	mStableIdToDenseId.erase(stableId);
	mFreeDenseIds.push(denseId);

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
