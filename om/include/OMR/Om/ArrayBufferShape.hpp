#if !defined(OMR_OM_ARRAYBUFFERSHAPE_HPP_)
#define OMR_OM_ARRAYBUFFERSHAPE_HPP_

#include <OMR/Om/Shape.hpp>
#include <OMR/Om/MetaShape.hpp>

#include <type_traits>

namespace OMR
{
namespace Om
{
struct MetaShape;

struct ArrayBufferShape
{
	union Base
	{
		Shape map;
		Cell cell;
	};

	static ArrayBufferShape* allocate(Context& cx);

	explicit ArrayBufferShape(MetaShape* meta);

	Base& base() noexcept { return base_; }

	const Base& base() const noexcept { return base_; }

	Shape& baseShape() noexcept { return base().map; }

	const Shape& baseShape() const noexcept { return base().map; }

	template <typename VisitorT>
	void visit(VisitorT& visitor)
	{
		base().map.visit(visitor);
	}

protected:
	Base base_;
};

static_assert(
	std::is_standard_layout<ArrayBufferShape>::value,
	"ArrayBufferShape must be a StandardLayoutType");

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_ARRAYBUFFERSHAPE_HPP_
