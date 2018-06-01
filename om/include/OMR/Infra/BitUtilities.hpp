#ifndef OMR_INFRA_BITUTILITIES_HPP_
#define OMR_INFRA_BITUTILITIES_HPP_

/// @file
/// Bit Utilities

namespace OMR {
namespace Infra {
template<typename T>
constexpr bool areAllBitsSet(T value, T mask) {
	return (value & mask) == mask;
}

template<typename T>
constexpr bool areAnyBitsSet(T value, T mask) {
	return (value & mask) != 0;
}

template<typename T>
constexpr bool areNoBitsSet(T value, T mask) {
	return (value & mask) == 0;
}

template<typename T>
constexpr bool isTagged(T value, T tag, T mask) {
	return (value & mask) == tag;
}

} // namespace Infra
} // namespace OMR

#endif // OMR_INFRA_BITUTILITIES_HPP_
