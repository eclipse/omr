#if !defined(OMR_OM_OBJECT_INL_HPP_)
#define OMR_OM_OBJECT_INL_HPP_

#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Barrier.hpp>
#include <OMR/Om/Object.hpp>
#include <OMR/Om/ShapeOperations.hpp>

namespace OMR {
namespace Om {

template<typename T = std::uint8_t*>
T slotAddress(Context& cx, Object* object, SlotIndex index) noexcept {
	return reinterpret_cast<T>(object->slotAddress(index));
}

/// Slot lookup by Id. The result is a Descriptor, which describes the slot's
/// location and type.
inline bool lookupSlot(Context& cx, const Object* object, Id id, SlotDescriptor& result) noexcept {
	for (const Shape& shape : object->layoutTree()) {
		for (const SlotDescriptor descriptor : shape.instanceSlotDescriptorRange()) {
			if (descriptor.attr().id() == id) {
				result = descriptor;
				return true;
			}
		}
	}
	return false;
}

/// Read a value from an object's slot.
inline Value getValue(Context& cx, Object* object, SlotIndex index) noexcept {
	return *slotAddress<Value*>(cx, object, index);
}

/// Store a value into an object's slot. Barriered.
inline void setValue(Context& cx, Object* object, SlotIndex index, Value value) noexcept {
	auto slotAddr = slotAddress<Value*>(cx, object, index);
	if (value.isRef()) {
		Barrier::writeReference(cx, object, ValueSlotHandle(slotAddr), value.getRef());
	} else {
		*slotAddr = value;
	}
}

/// Store a value into an object's inline slot. Barriered.
inline void setValueToInlineSlot(Context& cx, Object* self, SlotIndex index) noexcept { assert(0); }

/// Look up a layout, using a precomputed hash of the slots. Does not GC.
inline Shape* lookUpTransition(Context& cx,
                               const Object* object,
                               Infra::Span<const SlotAttr> attributes,
                               std::size_t hash) noexcept {
	return lookUpTransition(cx, object->layout(), attributes, hash);
}

/// Look up a layout from the slots to append. Does not GC.
inline Shape* lookUpTransition(Context& cx,
                               const Object* object,
                               Infra::Span<const SlotAttr> attributes) noexcept {
	return lookUpTransition(cx, object, attributes, hash(attributes));
}

/// Force set the layout of an object. Barriered.
inline void setLayout(Context& cx, Object* object, Shape* layout) noexcept {
	Barrier::writeReference(cx, object, CellHeaderLayoutHandle(&object->header()), layout);
}

/// Take an existing transition, without allocating. Barriered.
inline Shape* takeExistingTransition(Context& cx,
                                     Object* object,
                                     Infra::Span<const SlotAttr> attributes,
                                     std::size_t hash) {
	Shape* derivation = lookUpTransition(cx, object, attributes, hash);
	if (derivation != nullptr) {
		setLayout(cx, object, derivation);
	}
	return derivation;
}

/// Take an existing transition, without allocating. Barriered.
inline Shape*
takeExistingTransition(Context& cx, Object* object, Infra::Span<const SlotAttr> attributes) {
	return takeExistingTransition(cx, object, attributes, hash(attributes));
}

#if 0
inline Index Object::takeNewTransistion(Context& cx, Handle<Object> self, const SlotAttr& desc) {
  auto hash = desc.hash();
  return takeNewTransistion(cx, self, desc, hash);
}
#endif

// Take a layout/shape transition that hasn't been cached. Before taking a new
// transition, users should use `lookupTransition` to ensure the transition
// hasn't been taken before. See also `transition`, a higher level call for
// transitioning across object layouts. Barriered.
inline Shape* takeNewTransition(Context& cx,
                                Handle<Object> object,
                                Infra::Span<const SlotAttr> attributes,
                                std::size_t hash) {
	RootRef<Shape> base(cx, object->layout());
	Shape* derivation = deriveObjectLayout(cx, base, attributes, hash);
	setLayout(cx, object, derivation); // barrier
	return derivation;
}

/// Transition the object's shape by adding a set of new slots
/// This function will reuse cached transitions. Uses a precomputed hash for the attributes.
inline Shape* transitionLayout(Context& cx,
                               Handle<Object> object,
                               Infra::Span<const SlotAttr> attributes,
                               std::size_t hash) {
	Shape* derivation = takeExistingTransition(cx, object, attributes, hash);
	if (!derivation) {
		derivation = takeNewTransition(cx, object, attributes, hash);
	}
	return derivation;
}

/// Transition the object's shape by adding a set of new slots.
/// This function will reuse cached transitions.
inline Shape*
transitionLayout(Context& cx, Handle<Object> object, Infra::Span<const SlotAttr> attributes) {
	return transitionLayout(cx, object, attributes, hash(attributes));
}

/// Transition an object to a new layout. Barriered. Can GC.
/// TODO: allocate overflow slot storage?
inline Shape*
transitionLayout(Context& cx, Handle<Object> object, std::initializer_list<SlotAttr> list) {
	return transitionLayout(cx, object, Infra::Span<const SlotAttr>(list.begin(), list.size()));
}

struct ObjectInitializer : public Initializer {
	virtual Cell* operator()(Context& cx, Cell* cell) override {
		auto o = reinterpret_cast<Object*>(cell);
		o->layout(layout);
		return reinterpret_cast<Cell*>(o);
	}

	Handle<Shape> layout;
};

/// Allocate object with corresponding slot shape;
inline Object* allocateObject(Context& cx, Handle<Shape> layout) {
	ObjectInitializer init;
	init.layout = layout;
	return BaseAllocator::allocate<Object>(
	        cx, init, Object::cellSize(layout->instanceInlineSlotSize()));
}

/// Allocate an object with no slots.
inline Object* allocateEmptyObject(Context& cx, std::size_t inlineSlotsSize = 32 * sizeof(void*)) {
	RootRef<Shape> shape(cx, allocateRootObjectLayout(cx, {}, inlineSlotsSize));
	return allocateObject(cx, shape);
}

/// TODO: Implement cloneObject
inline Object* cloneObject(Context& cx, Handle<Object> base) { assert(0); }

} // namespace Om
} // namespace OMR

#endif // OMR_OM_OBJECT_INL_HPP_
