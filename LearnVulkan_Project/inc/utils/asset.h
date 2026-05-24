#pragma once
#include <filesystem>

#include "utils/type_defs.h"

class IAsset
{
private:
	AssetDefs::StableId mStableId;

public:
	IAsset(const std::filesystem::path& _filePath);

	AssetDefs::StableId GetStableId() const { return mStableId; }

private:
	AssetDefs::StableId StrToStableId(const std::string& _str) const;
};