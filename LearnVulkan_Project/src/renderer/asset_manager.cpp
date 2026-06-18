#include "pch.h"
#include "renderer/asset_manager.h"

#include "renderer/material.h"
#include "renderer/texture.h"

void AssetManager::OnCleanup() {
	// Run 'ClearMap' for each type specified in 'AllAssetTypes'
	std::apply([this](auto... assetTypes) {
		(ClearMap<typename decltype(assetTypes)::type>(), ...);
	}, AllAssetTypes{});

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
