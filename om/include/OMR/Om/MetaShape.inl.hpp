#if !defined(OMR_OM_METAMAPALLOCATOR_HPP_)
#define OMR_OM_METAMAPALLOCATOR_HPP_

#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/Initializer.hpp>
#include <OMR/Om/MetaShape.hpp>

namespace OMR
{
namespace Om
{
struct MetaMapInitializer final : public Initializer
{
	virtual Cell* operator()(Context& cx, Cell* cell) override
	{
		auto m = reinterpret_cast<MetaShape*>(cell);
		m->baseShape().map(m);  // m describes its own shape.
		m->baseShape().kind(Shape::Kind::META_SHAPE);
		return &m->baseCell();
	}
};

inline MetaShape*
MetaShape::allocate(Context& cx)
{
	MetaMapInitializer init;
	return BaseAllocator::allocate<MetaShape>(cx, init, sizeof(MetaShape));
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_METAMAPALLOCATOR_HPP_
