#pragma once
#include "utils/singleton.h"

#include <filesystem>
#include <functional>
#include <stack>

#include "utils/type_defs.h"

class Texture;

namespace AssetManagerGlobals {
	constexpr size_t gMaxTextureCount = 100;
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

class AssetManager : public ISingleton<AssetManager>
{
public:
	std::function<void(const Texture&, AssetDefs::DenseId, AssetDefs::TextureSlot)> onTextureLoaded;
	std::function<void(AssetDefs::TextureSlot)> onTextureUnloaded;

private:
	// Shared mapping, as stable ids are unique to asset paths
	std::unordered_map<AssetDefs::StableId, AssetDefs::DenseId> mStableIdToDenseId;

	// Texture mappings
	std::unordered_map<std::string, Texture*> mTextures;
	DenseToSlotAllocator<AssetDefs::TextureSlot, AssetManagerGlobals::gMaxTextureCount> mTextureSlotAllocator;

public:
	AssetManager() = default;
	~AssetManager();

	const Texture& LoadTexture(const std::filesystem::path& _path);
	void UnloadTexture(const std::string& _texPathStr);

	AssetDefs::DenseId GetDenseIdForTexture(const std::string& _texPathStr) const;
	bool IsTextureLoaded(const std::string& _texPathStr) const;
};