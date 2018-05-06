#if !defined(OMR_OM_ARRAYBUFFER_INL_HPP_)
#define OMR_OM_ARRAYBUFFER_INL_HPP_

#include <OMR/Om/ArrayBufferInitializer.hpp>

#include <OMR/Om/ArrayBuffer.inl.hpp>
#include <OMR/Om/Allocator.inl.hpp>

namespace OMR
{
namespace Om
{
inline ArrayBuffer::ArrayBuffer(ArrayBufferShape* map, std::size_t size)
	: baseCell_(&map->baseShape()), size_(size)
{
}

inline ArrayBuffer*
ArrayBuffer::allocate(Context& cx, std::size_t size)
{
	ArrayBufferInitializer init(size);
	return BaseAllocator::allocate<ArrayBuffer>(cx, init, calcAllocSize(size));
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_ARRAYBUFFER_INL_HPP_
