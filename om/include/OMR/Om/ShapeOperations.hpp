#if !defined(OMR_OM_SHAPEOPERATIONS_HPP_)
#define OMR_OM_SHAPEOPERATIONS_HPP_

#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/Handle.hpp>
#include <OMR/Om/Initializer.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/TransitionSetOperations.hpp>

#include <iostream>

namespace OMR {
namespace Om {

struct InstanceDescription {

};

inline bool
initializeTransitionSet(Context& cx, Handle<Shape> shape, std::size_t initialSetSize = 32) {
	assert(!shape->transitions_.initialized());
	return initializeTransitionSet(cx, {shape, &Shape::transitions_}, initialSetSize);
}

inline bool
storeTransition(Context& cx, Handle<Shape> shape, TransitionSetEntry entry, std::size_t hash) {
	return storeTransition(cx, {shape, &Shape::transitions_}, entry, hash);
}

/// A helper to copy a span of slot attributes into a shape.
/// Also calculates the total width of the slot attributes.
/// @internal
inline void assignSlotAttrIntoShape(Shape* shape, Infra::Span<const SlotAttr> attributes) {
	assert(shape->instanceSlotCount() == attributes.length());
	std::size_t instanceSlotWidth = 0;
	for (std::size_t i = 0; i < attributes.length(); i++) {
		shape->instanceSlotAttrs()[i] = attributes[i];
		instanceSlotWidth += attributes[i].type().width();
	}
	shape->instanceSlotWidth_ = instanceSlotWidth;
	shape->instanceSlotCount_ = attributes.length();
}

/// Initialize a root object layout shape.
struct RootObjectLayoutInitializer : public Initializer {
	virtual Cell* operator()(Context& cx, Cell* cell) noexcept override {
		Shape* shape = reinterpret_cast<Shape*>(cell);
		shape->layout(cx.globals().metaShape()); // TODO: Remove the MetaShape global
		shape->parentLayout_ = nullptr;
		shape->instanceSlotOffset_ = 0;
		shape->instanceSlotWidth_ = 0;
		shape->instanceSlotCount_ = attrs.length();
		shape->shapeTreeData_.instanceKind = CellKind::OBJECT;
		shape->shapeTreeData_.instanceInlineSlotsSize = instanceInlineSlotsSize;
		assignSlotAttrIntoShape(shape, attrs);
		return reinterpret_cast<Cell*>(shape);
	}

	Infra::Span<const SlotAttr> attrs;
	std::size_t instanceInlineSlotsSize = 0;
};

/// Allocate a shape that lays out an empty object. Starts a new ShapeTree.
inline Shape* allocateRootObjectLayout(Context& cx,
                                       Infra::Span<const SlotAttr> attrs,
                                       std::size_t instanceInlineSlotsSize = 32 * sizeof(void*)) {
	RootObjectLayoutInitializer init;
	init.attrs = attrs;
	init.instanceInlineSlotsSize = instanceInlineSlotsSize;
	std::size_t size = Shape::calculateCellSize(attrs.length());
	Shape* result = BaseAllocator::allocate<Shape>(cx, init, size);
	return result;
}

inline Shape* allocateRootObjectLayout(Context& cx, std::size_t instanceInlineSlotsSize = 32 * sizeof(void*)) {
	return allocateRootObjectLayout(cx, Infra::Span<const SlotAttr>(), instanceInlineSlotsSize);
}

/// A functor that performs basic initialization of an Shape that lays out an object.
struct ObjectLayoutInitializer : public Initializer {
	virtual Cell* operator()(Context& cx, Cell* cell) noexcept override {
		Shape* shape = reinterpret_cast<Shape*>(cell);
		shape->layout(cx.globals().metaShape());
		shape->parentLayout_ = parentLayout.get();
		shape->instanceSlotOffset_ = parentLayout->instanceSlotOffset_
		                             + parentLayout->instanceSlotWidth_;
		shape->shapeTreeData_ = parentLayout->shapeTreeData_;
		shape->instanceSlotCount_ = attributes.length();
		assignSlotAttrIntoShape(shape, attributes);
		return reinterpret_cast<Cell*>(shape);
	}

	Handle<Shape> parentLayout;
	Infra::Span<const SlotAttr> attributes;
};

/// Allocate a shape that lays out zero or more slots in an Object.
inline Shape* allocateObjectLayout(Context& cx,
                                   Handle<Shape> parentLayout,
                                   Infra::Span<const SlotAttr> attributes) {
	ObjectLayoutInitializer init;
	init.parentLayout = parentLayout;
	init.attributes = attributes;

	std::size_t size = Shape::calculateCellSize(attributes.length());
	Shape* result = BaseAllocator::allocate<Shape>(cx, init, size);
	return result;
}

/// An Intializer for a Shape that lays out other Shape objects.
struct ShapeLayoutInitializer : public Initializer {
	virtual Cell* operator()(Context& cx, Cell* cell) noexcept override {
		Shape* shape = reinterpret_cast<Shape*>(cell);
		shape->instanceKind(CellKind::SHAPE);
		shape->layout(shape);
		shape->parentLayout_ = cx.globals().metaShape();
		shape->instanceSlotOffset_ = 0;
		shape->instanceSlotCount_ = 0;
		shape->instanceSlotWidth_ = 0;
		return reinterpret_cast<Cell*>(shape);
	}
};

/// Allocate a Shape that lays out: a Shape with no slots. The resulting Shape is it's own layout.
inline Shape* allocateShapeLayout(Context& cx) {
	ShapeLayoutInitializer init;
	std::size_t cellSize = Shape::calculateCellSize(0); // no slot descriptors
	return BaseAllocator::allocate<Shape>(cx, init, cellSize);
}

/// An Intializer for a Shape that lays out other Shape objects.
struct ArrayLayoutInitializer : public Initializer {
	virtual Cell* operator()(Context& cx, Cell* cell) noexcept override {
		Shape* shape = reinterpret_cast<Shape*>(cell);
		shape->instanceKind(CellKind::ARRAY);
		shape->layout(cx.globals().metaShape());
		return reinterpret_cast<Cell*>(shape);
	}
};

/// Allocate a Shape that lays out: An Array with no slots.
inline Shape* allocateArrayLayout(Context& cx) {
	ArrayLayoutInitializer init;
	std::size_t cellSize = Shape::calculateCellSize(0); // no slot descriptors
	return BaseAllocator::allocate<Shape>(cx, init, cellSize);
}

/// Create a slot shape that derives base. Add the new slot shape to the set of
/// known transistions from base.
inline Shape* deriveObjectLayout(Context& cx,
                                 Handle<Shape> base,
                                 const Infra::Span<const SlotAttr>& attr,
                                 std::size_t hash) {
	if (!base->transitions_.initialized()) {
		initializeTransitionSet(cx, {base, &Shape::transitions_});
	}
	Shape* derivation = allocateObjectLayout(cx, base, attr);
	storeTransition(cx, base, TransitionSetEntry{derivation}, hash);
	std::cerr << "derivation: " << derivation << std::endl;
	return derivation;
}

/// Look up a transition to a derived shape.
inline Shape* lookUpTransition(Context& cx,
                               Shape* shape,
                               const Infra::Span<const SlotAttr>& attrs,
                               std::size_t hash) {
	return lookUpTransition(shape->transitions_, attrs, hash);
}

#if 0

inline bool
tryStoreTransition(Shape* shape, Shape* transitionTarget, std::size_t hash)
{
	return tryStoreTransition(shape->transitionSet, entry, hash);
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
#endif // 0

} // namespace Om
} // namespace OMR

#endif // OMR_OM_SHAPEOPERATIONS_HPP_
