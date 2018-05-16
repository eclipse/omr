#if !defined(OMR_OM_SHAPEOPERATIONS_HPP_)
#define OMR_OM_SHAPEOPERATIONS_HPP_

#include <OMR/Om/Shape.hpp>
#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/TransitionSet.inl.hpp>
#include <OMR/Om/Initializer.hpp>
#include <OMR/Om/Handle.hpp>

namespace OMR
{
namespace Om
{

/// A functor that performs basic initialization of an Shape.
struct ShapeInitializer : public Initializer
{
	virtual Cell *operator()(Context &cx, Cell *cell) noexcept override
	{
		auto meta = cx.globals().metaShape();
		new (cell) Shape(meta, parent, attributes);
		return cell;
	}

	Handle<Shape> parentLayout;
	Infra::Span<const SlotAttr> attributes;
};

/// Allocate an object shape that describes one slot.
inline Shape *allocateObjectLayout(Context &cx, Handle<Shape> parentLayout, Infra::Span<const SlotAttr> attributes)
{
	ShapeInitializer init;
	init.parent = parent;
	init.attributes = attributes;

	std::size_t size = Shape::calculateAllocSize(attributes.length());
	Shape *result = BaseAllocator::allocate<Shape>(cx, init, size);

	RootRef<Shape> root(cx, result);
	bool ok = construct(cx, root);
	if (!ok)
	{
		return result = nullptr;
	}
	else
	{
		result = root.get();
	}

	return result;
}

/// An Intializer for a Shape that lays out other Shape objects.
struct ShapeLayoutInitializer : public Intializer
{
	virtual Cell *operator()(Context &cx, Cell *cell) noexcept override {
		Shape* shape = reinterpret_cast<Shape*>(cell);
		shape->instanceKind(CellKind::Shape);
		shape->layout(shape);
		return reinterpret_cast<Cell*>(shape);
	}
}

/// Allocate a Shape that lays out: A shape with no slots.
inline Shape *
allocateShapeLayout(Context &cx)
{
	ShapeLayoutInitializer init;
	std::size_t cellSize = Shape::calculateAllocSize(0); // no slot descriptors
	BaseAllocator::allocate<Shape>(cx, init, size);
}

/// An Intializer for a Shape that lays out other Shape objects.
struct ArrayLayoutInitializer : public Intializer
{
	virtual Cell *operator()(Context &cx, Cell *cell) noexcept override {
		Shape* shape = reinterpret_cast<Shape*>(cell);
		shape->instanceKind(CellKind::Array);
		shape->layout(cx.globals().metaShape());
		return reinterpret_cast<Cell*>(shape);
	}
}

/// Allocate a Shape that lays out: An Array with no slots.
inline Shape *
allocateArrayLayout(Context &cx)
{
	ArrayLayoutInitializer init;
	std::size_t cellSize = Shape::calculateAllocSize(0); // no slot descriptors
	BaseAllocator::allocate<Shape>(cx, init, size);
}

/// Allocate an object shape that describes no slots
inline Shape *allocateEmptyShape(Context &cx)
{
	// TODO: Don't construct a bogus handle here, find a clearer way to do this.
	Shape *parent = nullptr;
	return allocate(cx, Handle<Shape>(parent), Infra::Span<const SlotAttr>(nullptr, 0));
}

/// Part two of initialization. @see-also ShapeInitializer
inline bool constructShape(Context &cx, Handle<Shape> self)
{
	return TransitionSet::construct(cx, {self, &Shape::transitions_});
}

inline bool constructShape(
	Shape *meta, Shape *parent, const Infra::Span<const SlotAttr> &attributes)
	: baseMap_(meta, Shape::Kind::OBJECT_MAP), parent_(parent), transitions_(),
	  // TODO: Find a clearer way to construct a shape without a parent.
	  slotOffset_(parent ? (parent->slotOffset() + parent->slotWidth()) : 0), slotWidth_(0), slotCount_(attributes.length())
{
	for (std::size_t i = 0; i < slotCount_; i++)
	{
		attributes_[i] = attributes[i];
		slotWidth_ += attributes[i].type().width();
	}
}

/// Create a slot shape that derives base. Add the new slot shape to the set of
/// known transistions from base.
inline Shape *
deriveShape(Context &cx, Handle<Shape> base, const Infra::Span<const SlotAttr> &attr,
			std::size_t hash)
{
	Shape *derivation = Shape::allocate(cx, base, attributes);
	base->transitions_.tryStore(derivation, hash);
	// TODO: Write barrier? the object shape
	return derivation;
}

} // namespace Om

/// Look up a transition to a derived shape.
inline Shape *
lookUpTransition(Context &cx, Shape *shape, const Infra::Span<const SlotAttr> &attr, std::size_t hash)
{
	return shape->transitions_.lookup(attributes, hash);
}

inline ArrayBufferShape::ArrayBufferShape(MetaShape *meta) : base_{{meta, Shape::Kind::ARRAY_BUFFER_MAP}} {}

struct ArrayBufferMapInitializer : public Initializer
{
	Cell *operator()(Context &cx, Cell *cell)
	{
		auto meta = cx.globals().metaShape();
		new (cell) ArrayBufferShape(meta);
		return cell;
	}
};

inline ArrayBufferShape *
ArrayBufferShape::allocate(Context &cx)
{
	ArrayBufferMapInitializer init;
	return BaseAllocator::allocate<ArrayBufferShape>(cx, init);
}

} // namespace OMR
} // namespace OMR

#endif // OMR_OM_SHAPEOPERATIONS_HPP_
