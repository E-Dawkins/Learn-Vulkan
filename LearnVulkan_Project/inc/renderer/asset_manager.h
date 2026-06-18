#pragma once
#include "interfaces/singleton.h"

#include "interfaces/asset.h"
#include "utils/type_defs.h"

#include <stack>

class Material;
class Mesh;
class Texture;

template<typename T>
concept ValidAssetType =
	std::is_same_v<T, Texture> ||
	std::is_same_v<T, Material> ||
	std::is_same_v<T, Mesh>;

using AllAssetTypes = std::tuple<
	std::type_identity<Texture>,
	std::type_identity<Material>,
	std::type_identity<Mesh>
>;

namespace AssetManagerGlobals {
	struct AssetTraitConfig
	{
		uint32_t maxCount = 0;
		const char* defaultAsset = "";
	};

	template<ValidAssetType T>
	struct AssetTraits
	{
		// Default traits has no slot type
		static constexpr AssetTraitConfig config;
	};

	template<>
	struct AssetTraits<Texture>
	{
		using SlotType = AssetDefs::TextureSlot;
		static constexpr AssetTraitConfig config = {
			.maxCount = 100,
			.defaultAsset = "textures\\default_texture.png"
		};
	};

	template<>
	struct AssetTraits<Material>
	{
		using SlotType = AssetDefs::MaterialSlot;
		static constexpr AssetTraitConfig config = {
			.maxCount = 100
		};
	};
}

template<typename T>
concept HasSlotType = requires {
	typename AssetManagerGlobals::AssetTraits<T>::SlotType;
};

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

class AssetManager : public ISingleton<AssetManager>
{
private:
	// Shared mapping, as stable ids are unique to asset paths
	std::unordered_map<AssetDefs::StableId, std::shared_ptr<IAsset>> mStableIdToAsset;

private:
	template<ValidAssetType T>
	struct MapFor
	{
		static inline std::unordered_map<std::string, std::shared_ptr<T>> value;
	};

	template<HasSlotType T>
	struct AllocatorFor
	{
		using TraitsType = AssetManagerGlobals::AssetTraits<T>;

		static inline DenseToSlotAllocator<typename TraitsType::SlotType, TraitsType::config.maxCount> value;
	};

	template<typename T>
	struct CallbacksFor;

	template<HasSlotType T>
	struct CallbacksFor<T>
	{
		using TraitsType = AssetManagerGlobals::AssetTraits<T>;

		static inline std::function<void(std::weak_ptr<T>, AssetDefs::DenseId, typename TraitsType::SlotType)> loadCallback;
		static inline std::function<void(AssetDefs::DenseId)> unloadCallback;
	};

	template<typename T>
		requires (ValidAssetType<T> && !HasSlotType<T>)
	struct CallbacksFor<T>
	{
		static inline std::function<void(std::weak_ptr<T>)> loadCallback;
	};

public:
	void OnCleanup() override;

private:
	std::string StripFirstFolder(const std::filesystem::path& _filepath);

	template<ValidAssetType T>
	void ClearMap() {
		MapFor<T>::value.clear();
	}

public:
	bool IsAssetLoaded(AssetDefs::StableId _stableId) const;

	template<ValidAssetType T>
	bool IsAssetLoaded(const std::string& _pathStr) const {
		auto& assetMap = MapFor<T>::value;
		return assetMap.contains(_pathStr);
	}

	template<ValidAssetType T>
	std::weak_ptr<T> GetAsset(AssetDefs::StableId _stableId) const {
		assert(IsAssetLoaded(_stableId));

		auto& assetSharedPtr = mStableIdToAsset.at(_stableId);
		assert(assetSharedPtr);

		return std::dynamic_pointer_cast<T>(assetSharedPtr);
	}

	template<ValidAssetType T>
	std::weak_ptr<T> GetAsset(const std::string& _pathStr) const {
		assert(IsAssetLoaded<T>(_pathStr));

		auto& assetSharedPtr = MapFor<T>::value.at(_pathStr);
		assert(assetSharedPtr);

		return std::dynamic_pointer_cast<T>(assetSharedPtr);
	}

	template<ValidAssetType T>
	std::weak_ptr<T> LoadAsset(const std::filesystem::path& _filepath) {
		const std::string assetName = StripFirstFolder(_filepath);
		
		// Store in asset map
		auto& assetMap = MapFor<T>::value;
		assetMap[assetName] = std::make_shared<T>(_filepath);
		auto& loadedAsset = assetMap[assetName];

		// Store stable id -> asset mapping
		mStableIdToAsset[loadedAsset->GetStableId()] = assetMap[assetName];

		if constexpr (HasSlotType<T>) {
			// Allocate a dense id
			auto& slotAllocator = AllocatorFor<T>::value;
			AssetDefs::DenseId denseId = slotAllocator.AllocateId();
			loadedAsset->mDenseId = denseId;

			auto& callback = CallbacksFor<T>::loadCallback;
			if (callback) {
				callback(loadedAsset, denseId, slotAllocator.GetSlotForId(denseId));
			}
		}
		else {
			auto& callback = CallbacksFor<T>::loadCallback;
			if (callback) {
				callback(loadedAsset);
			}
		}

		return std::dynamic_pointer_cast<T>(loadedAsset);
	}

	template<ValidAssetType T>
	void UnloadAsset(const std::string& _pathStr) {
		auto& assetMap = MapFor<T>::value;

		// No asset found with passed in string
		if (!assetMap.contains(_pathStr)) {
			return;
		}

		// Do not allow unloading the default asset
		const std::string defaultAssetStr = AssetManagerGlobals::AssetTraits<T>::config.defaultAsset;
		if (!defaultAssetStr.empty() && _pathStr == defaultAssetStr) {
			std::cerr << "Someone tried to unload '" << defaultAssetStr << "' ... naughty!\n";
			return;
		}

		// Asset already null, just remove it from map
		if (!assetMap[_pathStr]) {
			assetMap.erase(_pathStr);
			return;
		}

		std::shared_ptr<T>& assetToUnload = assetMap[_pathStr];
		AssetDefs::StableId stableId = assetToUnload->mStableId;
		mStableIdToAsset.erase(stableId);

		// Only assets with slots (& dense id) have an unload callback
		if constexpr (HasSlotType<T>) {
			AssetDefs::DenseId denseId = assetToUnload->mDenseId;
			AllocatorFor<T>::value.FreeId(denseId);

			auto& callback = CallbacksFor<T>::unloadCallback;
			if (callback) {
				callback(denseId);
			}
		}

		assetMap.erase(_pathStr);
	}

	template<ValidAssetType T>
	auto& GetLoadCallback() const {
		return CallbacksFor<T>::loadCallback;
	}

	template<HasSlotType T>
	auto& GetUnloadCallback() const {
		return CallbacksFor<T>::unloadCallback;
	}
};