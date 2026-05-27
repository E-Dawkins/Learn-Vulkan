#pragma once
#include "utils/singleton.h"
#include "utils/type_defs.h"

#include "utils/asset.h"

#include <stack>

class Material;
class Texture;

namespace AssetManagerGlobals {
	constexpr size_t gMaxTextureCount = 100;
	constexpr size_t gMaxMaterialCount = 100;
}

template<typename SlotType, size_t Count>
struct DenseToSlotAllocator
{
private:
	std::unordered_map<AssetDefs::DenseId, SlotType> mDenseIdToSlot;
	std::stack<AssetDefs::DenseId> mFreeDenseIds;
	std::stack<SlotType> mFreeSlots;

public:
	DenseToSlotAllocator() {
		for (size_t i = 0; i < Count; i++) {
			size_t current = Count - i - 1;

			mFreeDenseIds.push(static_cast<AssetDefs::DenseId>(current));
			mFreeSlots.push(static_cast<SlotType>(current));
		}
	}

	AssetDefs::DenseId AllocateId() {
		// Grab next available dense id
		assert(!mFreeDenseIds.empty());
		AssetDefs::DenseId denseId = mFreeDenseIds.top();
		mFreeDenseIds.pop();

		// Grab next available slot
		assert(!mFreeSlots.empty());
		SlotType slot = mFreeSlots.top();
		mFreeSlots.pop();

		// Store dense id -> slot mapping
		mDenseIdToSlot[denseId] = slot;

		return denseId;
	}

	void FreeId(AssetDefs::DenseId _id) {
		assert(mDenseIdToSlot.contains(_id));

		// Erase _id -> slot mapping
		SlotType slot = mDenseIdToSlot[_id];
		mDenseIdToSlot.erase(_id);

		// Push _id & slot back into free stacks
		mFreeDenseIds.push(_id);
		mFreeSlots.push(slot);
	}

	const SlotType& GetSlotForId(AssetDefs::DenseId _id) const {
		assert(mDenseIdToSlot.contains(_id));
		return mDenseIdToSlot.at(_id);
	}
};

template<typename AssetType, typename SlotType>
using LoadCallback = std::function<void(AssetType&, AssetDefs::DenseId, SlotType)>;
using UnloadCallback = std::function<void(AssetDefs::DenseId)>;

template<typename AssetType>
using AssetMap = std::unordered_map<std::string, AssetType*>;

template<typename T>
concept ValidAssetType =
	std::is_same_v<T, Texture> ||
	std::is_same_v<T, Material>;

class AssetManager : public ISingleton<AssetManager>
{
public:
	LoadCallback<Texture, AssetDefs::TextureSlot> onTextureLoaded;
	UnloadCallback onTextureUnloaded;

	LoadCallback<Material, AssetDefs::MaterialSlot> onMaterialLoaded;
	UnloadCallback onMaterialUnloaded;

private:
	// Shared mapping, as stable ids are unique to asset paths
	std::unordered_map<AssetDefs::StableId, IAsset*> mStableIdToAsset;

	// Texture mappings
	AssetMap<Texture> mTextures;
	DenseToSlotAllocator<AssetDefs::TextureSlot, AssetManagerGlobals::gMaxTextureCount> mTextureSlotAllocator;

	// Material mappings
	std::unordered_map<std::string, Material*> mMaterials;
	DenseToSlotAllocator<AssetDefs::MaterialSlot, AssetManagerGlobals::gMaxMaterialCount> mMaterialSlotAllocator;

public:
	AssetManager() = default;
	~AssetManager();

private:
	std::string StripFirstFolder(const std::filesystem::path& _path);

	template<typename AssetType, typename SlotType, size_t AssetCount>
	AssetType& TryLoadAsset(const std::filesystem::path& _path, AssetMap<AssetType>& _assetMap, DenseToSlotAllocator<SlotType, AssetCount>& _slotAllocator, LoadCallback<AssetType, SlotType> _callback = {}) {
		const std::string assetName = StripFirstFolder(_path);

		// Store in asset map
		_assetMap[assetName] = new AssetType(_path);
		AssetType* loadedAsset = _assetMap[assetName];

		// Allocate a dense id
		AssetDefs::DenseId denseId = _slotAllocator.AllocateId();
		loadedAsset->mDenseId = denseId;

		// Store stable -> dense id mapping
		mStableIdToAsset[loadedAsset->GetStableId()] = loadedAsset;

		// Optional callback
		if (_callback) {
			_callback(*loadedAsset, denseId, _slotAllocator.GetSlotForId(denseId));
		}

		return *loadedAsset;
	}

	template<typename AssetType, typename SlotType, size_t AssetCount>
	void TryUnloadAsset(const std::string& _pathStr, AssetMap<AssetType>& _assetMap, DenseToSlotAllocator<SlotType, AssetCount>& _slotAllocator, const std::string& _defaultAssetStr = "", UnloadCallback _callback = {}) {
		// No asset found with passed in string
		if (!_assetMap.contains(_pathStr)) {
			return;
		}

		// Do not allow unloading the default asset
		if (!_defaultAssetStr.empty() && _pathStr == _defaultAssetStr) {
			std::cerr << "Someone tried to unload '" << _defaultAssetStr << "' ... naughty!\n";
			return;
		}

		AssetType* assetToUnload = _assetMap[_pathStr];
		if (!assetToUnload) {
			// Asset already null, just remove it from map
			_assetMap.erase(_pathStr);
			return;
		}

		// Retrieve all stored ids
		AssetDefs::StableId stableId = assetToUnload->mStableId;
		AssetDefs::DenseId denseId = assetToUnload->mDenseId;

		// Free mappings
		_slotAllocator.FreeId(denseId);
		mStableIdToAsset.erase(stableId);

		// Remove asset from asset map
		_assetMap.erase(_pathStr);
		delete assetToUnload;

		// Optional callback
		if (_callback) {
			_callback(denseId);
		}
	}

	template<typename AssetType>
	AssetType& TryGetAsset(const std::string& _pathStr, const AssetMap<AssetType>& _assetMap) const {
		assert(_assetMap.contains(_pathStr));

		AssetType* asset = _assetMap.at(_pathStr);
		assert(asset);

		return *asset;
	}

public:
	template<ValidAssetType AssetType>
	AssetType& GetAssetFromStableId(AssetDefs::StableId _stableId) const {
		assert(mStableIdToAsset.contains(_stableId));
		return *dynamic_cast<AssetType*>(mStableIdToAsset.at(_stableId));
	}

	template<ValidAssetType AssetType>
	AssetType& GetAssetFromPath(const std::string& _pathStr) const {
		if constexpr (std::is_same_v<AssetType, Texture>) {
			return TryGetAsset(_pathStr, mTextures);
		}
		else if constexpr (std::is_same_v<AssetType, Material>) {
			return TryGetAsset(_pathStr, mMaterials);
		}
	}

	template<ValidAssetType AssetType>
	AssetType& LoadAsset(const std::filesystem::path& _path) {
		if constexpr (std::is_same_v<AssetType, Texture>) {
			return TryLoadAsset(_path, mTextures, mTextureSlotAllocator, onTextureLoaded);
		}
		else if constexpr (std::is_same_v<AssetType, Material>) {
			return TryLoadAsset(_path, mMaterials, mMaterialSlotAllocator, onMaterialLoaded);
		}
	}

	template<ValidAssetType AssetType>
	void UnloadAsset(const std::string& _pathStr) {
		if constexpr (std::is_same_v<AssetType, Texture>) {
			return TryUnloadAsset(_pathStr, mTextures, mTextureSlotAllocator, "textures\\default_texture.png", onTextureUnloaded);
		}
		else if constexpr (std::is_same_v<AssetType, Material>) {
			return TryUnloadAsset(_pathStr, mMaterials, mMaterialSlotAllocator, "", onMaterialUnloaded);
		}
	}

	template<ValidAssetType AssetType>
	bool IsAssetLoaded(const std::string& _pathStr) const {
		if constexpr (std::is_same_v<AssetType, Texture>) {
			return mTextures.contains(_pathStr);
		}
		else if constexpr (std::is_same_v<AssetType, Material>) {
			return mMaterials.contains(_pathStr);
		}
	}

	bool IsAssetLoaded(AssetDefs::StableId _stableId) const;
};