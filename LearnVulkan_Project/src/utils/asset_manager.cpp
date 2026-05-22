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
	mFreeTextureSlots.pop();

	// Finally, call onTextureLoaded callback
	if (onTextureLoaded) {
		onTextureLoaded(loadedTex, denseId, textureSlot);
	}

	return loadedTex;
}

AssetDefs::DenseId AssetManager::GetDenseIdForTexture(const std::string& _texPathStr) const {
	assert(mTextures.contains(_texPathStr));

	Texture* tex = mTextures.at(_texPathStr);
	assert(tex);

	return mStableIdToDenseId.at(tex->GetStableId());
}
