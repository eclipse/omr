#ifndef OMR_OM_SHAPE_HPP_
#define OMR_OM_SHAPE_HPP_

#include <OMR/Om/Cell.hpp>

namespace OMR
{
namespace Om
{
class Context;
struct MetaShape;

/// A description of a cell's layout, or shape. The Shape is akin to a java class,
/// except that Maps are typically very small. Every Cell has a Shape. Maps may be
/// shared by Cells. The MapKind can be examined to tell what kind of thing the
/// cell is.
struct Shape
{
	enum class Kind
	{
		META_SHAPE,
		OBJECT_MAP,
		ARRAY_BUFFER_MAP
	};

	union Base
	{
		Cell cell;
	};

	static void construct(Context& cx, Shape* self, MetaShape* meta, Kind kind);

	Shape(MetaShape* meta, Kind kind);

	Base& base() { return base_; }

	const Base& base() const { return base_; }

	Cell& baseCell() { return base().cell; }

	const Cell& baseCell() const { return base().cell; }

	MetaShape* map() const { return reinterpret_cast<MetaShape*>(baseCell().map()); }

	Shape& map(MetaShape* m)
	{
		baseCell().map(reinterpret_cast<Shape*>(m));
		return *this;
	}

	Kind kind() const { return kind_; }

	Shape& kind(Kind k)
	{
		kind_ = k;
		return *this;
	}

	template <typename VisitorT>
	void visit(VisitorT& visitor)
	{
		baseCell().visit(visitor);
	}

	Base base_;
	Kind kind_;
};

static_assert(std::is_standard_layout<Shape>::value, "Shape must be a StandardLayoutType.");

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_SHAPE_HPP_
