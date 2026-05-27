#include "pch.h"
#include "utils/asset_manager.h"

#include "renderer/material.h"
#include "renderer/texture.h"

AssetManager::~AssetManager() {
	for (auto& [path, tex] : mTextures) {
		delete tex;
	}

	for (auto& [path, mat] : mMaterials) {
		delete mat;
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
