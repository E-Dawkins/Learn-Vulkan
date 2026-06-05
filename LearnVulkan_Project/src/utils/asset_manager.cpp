#include "pch.h"
#include "utils/asset_manager.h"

#include "renderer/material.h"
#include "renderer/texture.h"

AssetManager::~AssetManager() {
	for (auto& [path, tex] : MapFor<Texture>::value) {
		delete tex;
	}

	for (auto& [path, mat] : MapFor<Material>::value) {
		delete mat;
	}

	mStableIdToAsset.clear();
}

std::string AssetManager::StripFirstFolder(const std::filesystem::path& _path) {
	std::filesystem::path relativePath;
	for (std::filesystem::path::iterator itr = (++_path.begin()); itr != _path.end(); ++itr) {
		relativePath /= *itr;
	}

	return relativePath.string();
}

bool AssetManager::IsAssetLoaded(AssetDefs::StableId _stableId) const {
	return mStableIdToAsset.contains(_stableId);
}
