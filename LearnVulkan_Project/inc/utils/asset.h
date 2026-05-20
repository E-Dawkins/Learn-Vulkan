#pragma once
#include <filesystem>

class IAsset
{
private:
	uint64_t mStableId;

public:
	IAsset(const std::filesystem::path& _filePath);

	uint64_t GetStableId() const { return mStableId; }

private:
	uint64_t StrToStableId(const std::string& _str) const;
};