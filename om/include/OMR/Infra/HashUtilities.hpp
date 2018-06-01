#if !defined(OMR_INFRA_HASHING_HPP_)
#define OMR_INFRA_HASHING_HPP_

#include <cstddef>

namespace OMR {
namespace Infra {
using HashNumber = std::size_t;

namespace Hash {
/// Combine two hashes to form a new hash.
/// TODO: Write a real hash-mixing function
constexpr HashNumber mix(HashNumber a, HashNumber b) { return ((a ^ b) << 3) * 7; }

template<typename T>
constexpr HashNumber hash(const T& x) {
	return mix(x, x + 5);
}

} // namespace Hash
} // namespace Infra
} // namespace OMR

#endif // OMR_INFRA_HASHING_HPP_
