#if !defined(OMR_OM_METAMAP_HPP_)
#define OMR_OM_METAMAP_HPP_

#include <OMR/Om/Shape.hpp>

#include <type_traits>

namespace OMR
{
namespace Om
{
/// A map that describes the shape of other Maps. The MetaShape is self
/// descriptive, IE metaShape->map() == metaShape. The MetaShape is a heap-wide
/// singleton.
struct MetaShape
{
	union Base
	{
		Shape map;
		Cell cell;
	};

	static MetaShape* allocate(Context& cx);

	Base& base() { return base_; }

	const Base& base() const { return base_; }

	Cell& baseCell() { return base().cell; }

	const Cell& baseCell() const { return base().cell; }

	Shape& baseShape() { return base().map; }

	const Shape& baseShape() const { return base().map; }

	MetaShape* map() const { return baseShape().map(); }

	template <typename VisitorT>
	void visit(VisitorT& visitor)
	{
		baseShape().visit(visitor);
	}

	Base base_;
};

static_assert(std::is_standard_layout<MetaShape>::value, "MetaShape must be a StandardLayoutType");

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_METAMAP_HPP_
