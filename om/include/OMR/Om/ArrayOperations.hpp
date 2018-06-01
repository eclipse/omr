#if !defined(OMR_OM_ARRAYOPERATIONS_HPP_)
#define OMR_OM_ARRAYOPERATIONS_HPP_

#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Array.hpp>
#include <OMR/Om/ArrayInitializer.hpp>

namespace OMR {
namespace Om {

static std::size_t arrayAllocationSize(std::size_t dataSize) { return sizeof(Array) + dataSize; }

/// Allocate a new Array<T>. GC Safepoint. size is the number of
/// elements, not the size in bytes of the buffer.
inline Array* allocateArray(Context& cx, std::size_t dataSize) {
	ArrayInitializer init(dataSize);
	return BaseAllocator::allocate<Array>(cx, init, arrayAllocationSize(dataSize));
}

} // namespace Om
} // namespace OMR

#endif // OMR_OM_ARRAYOPERATIONS_HPP_
