#include "pch.h"
#include "renderer/asset_manager.h"

#include "renderer/material.h"
#include "renderer/texture.h"

AssetManager::AssetManager(const std::filesystem::path& _assetFolder)
	: mAssetFolder(_assetFolder) {}

void AssetManager::OnInitialized() {
	LOG_MSG("Init", LogVerbosity::Info);
}

void AssetManager::OnCleanup() {
	mIsCleaningUp = true;

	// Run 'ClearMap' for each type specified in 'AllAssetTypes'
	std::apply([this](auto... assetTypes) {
		(ClearMap<typename decltype(assetTypes)::type>(), ...);
	}, AllAssetTypes{});

	mStableIdToAsset.clear();

	LOG_MSG("Cleanup", LogVerbosity::Info);
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
