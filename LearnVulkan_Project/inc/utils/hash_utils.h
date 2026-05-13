#pragma once

namespace Utils {
	namespace HashUtils {
		constexpr void HashCombine(size_t& _seed, size_t _value) {
			// Golden ratio, results in a more evenly distributed hash
			constexpr size_t kMul = 0x9e3779b97f4a7c15ULL;

			_seed ^= _value + kMul + (_seed << 6) + (_seed >> 2);
		}
	}
}