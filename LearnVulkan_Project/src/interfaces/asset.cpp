#include "pch.h"
#include "interfaces/asset.h"

IAsset::IAsset(const std::filesystem::path& _filePath) {
	mStableId = StrToStableId(_filePath.string());
}

uint64_t IAsset::StrToStableId(const std::string& _str) const {
	// This function and constants were taken directly from:
	// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function

	constexpr uint64_t fnvOffsetBasis = 14695981039346656037;
	constexpr uint64_t fnvPrime = 1099511628211;

	uint64_t hash = fnvOffsetBasis;

	for (auto& c : _str) {
		hash ^= static_cast<uint8_t>(c);
		hash *= fnvPrime;
	}

	return hash;
}
