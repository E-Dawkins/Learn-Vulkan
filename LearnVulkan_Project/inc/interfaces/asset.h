#pragma once
#include <filesystem>

#include "utils/type_defs.h"

class IAsset
{
	friend class AssetManager;

private:
	AssetDefs::StableId mStableId;
	AssetDefs::DenseId mDenseId;

public:
	IAsset(const std::filesystem::path& _filePath);
	virtual ~IAsset() = default;

	const AssetDefs::StableId& GetStableId() const { return mStableId; }
	const AssetDefs::DenseId& GetDenseId() const { return mDenseId; }

private:
	AssetDefs::StableId StrToStableId(const std::string& _str) const;
};