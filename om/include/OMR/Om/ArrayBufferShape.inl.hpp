#if !defined(OMR_OM_ARRAYBUFFERMAP_INL_HPP_)
#define OMR_OM_ARRAYBUFFERMAP_INL_HPP_

#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Shape.inl.hpp>

namespace OMR
{
namespace Om
{
inline ArrayBufferShape::ArrayBufferShape(MetaShape* meta) : base_{{meta, Shape::Kind::ARRAY_BUFFER_MAP}} {}

struct ArrayBufferMapInitializer : public Initializer
{
	Cell* operator()(Context& cx, Cell* cell)
	{
		auto meta = cx.globals().metaShape();
		new (cell) ArrayBufferShape(meta);
		return cell;
	}
};

inline ArrayBufferShape*
ArrayBufferShape::allocate(Context& cx)
{
	ArrayBufferMapInitializer init;
	return BaseAllocator::allocate<ArrayBufferShape>(cx, init);
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_ARRAYBUFFERMAP_INL_HPP_
