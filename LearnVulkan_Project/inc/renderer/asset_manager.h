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
	std::unordered_map<AssetDefs::StableId, IAsset*> mStableIdToAsset;

private:
	template<ValidAssetType T>
	struct MapFor
	{
		static inline std::unordered_map<std::string, T*> value;
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

		static inline std::function<void(T&, AssetDefs::DenseId, typename TraitsType::SlotType)> loadCallback;
		static inline std::function<void(AssetDefs::DenseId)> unloadCallback;
	};

	template<typename T>
		requires (ValidAssetType<T> && !HasSlotType<T>)
	struct CallbacksFor<T>
	{
		static inline std::function<void(T&)> loadCallback;
	};

public:
	~AssetManager();

private:
	std::string StripFirstFolder(const std::filesystem::path& _filepath);

public:
	bool IsAssetLoaded(AssetDefs::StableId _stableId) const;

	template<ValidAssetType T>
	bool IsAssetLoaded(const std::string& _pathStr) const {
		auto& assetMap = MapFor<T>::value;
		return assetMap.contains(_pathStr);
	}

	template<ValidAssetType T>
	T& GetAsset(AssetDefs::StableId _stableId) const {
		assert(IsAssetLoaded(_stableId));

		T* assetPtr = dynamic_cast<T*>(mStableIdToAsset.at(_stableId));
		assert(assetPtr);

		return *assetPtr;
	}

	template<ValidAssetType T>
	T& GetAsset(const std::string& _pathStr) const {
		assert(IsAssetLoaded<T>(_pathStr));

		T* assetPtr = MapFor<T>::value.at(_pathStr);
		assert(assetPtr);

		return *assetPtr;
	}

	template<ValidAssetType T>
	T& LoadAsset(const std::filesystem::path& _filepath) {
		const std::string assetName = StripFirstFolder(_filepath);
		
		// Store in asset map
		auto& assetMap = MapFor<T>::value;
		assetMap[assetName] = new T(_filepath);
		T* loadedAsset = assetMap[assetName];

		// Store stable id -> asset mapping
		mStableIdToAsset[loadedAsset->GetStableId()] = loadedAsset;

		if constexpr (HasSlotType<T>) {
			// Allocate a dense id
			auto& slotAllocator = AllocatorFor<T>::value;
			AssetDefs::DenseId denseId = slotAllocator.AllocateId();
			loadedAsset->mDenseId = denseId;

			auto& callback = CallbacksFor<T>::loadCallback;
			if (callback) {
				callback(*loadedAsset, denseId, slotAllocator.GetSlotForId(denseId));
			}
		}
		else {
			auto& callback = CallbacksFor<T>::loadCallback;
			if (callback) {
				callback(*loadedAsset);
			}
		}

		return *loadedAsset;
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
		T* assetToUnload = assetMap[_pathStr];
		if (!assetToUnload) {
			assetMap.erase(_pathStr);
			return;
		}

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
		delete assetToUnload;
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