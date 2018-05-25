#if !defined(OMR_OM_OBJECT_INL_HPP_)
#define OMR_OM_OBJECT_INL_HPP_

#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Object.hpp>
#include <OMR/Om/ShapeOperations.hpp>

namespace OMR
{
namespace Om
{

/// TODO: Now that we support multiple CoreTypes, the result is not always the
/// same type. We should be returning the type of slot as well.
inline bool
Object::lookup(Context& cx, const Object* self, Id id, SlotDescriptor& result)
{
	for (const Shape& shape : self->layoutTree()) {
		for (const SlotDescriptor descriptor : shape.instanceSlotDescriptorRange()) {
			if (descriptor.attr().id() == id) {
				result = descriptor;
				return true;
			}
		}
	}
	return false;
}

inline Value
Object::getValue(const Object* self, SlotIndex index) noexcept
{
	Value* pointer = (Value*)(self->fixedSlots_ + index.offset());
	return *pointer;
}

inline void
Object::setValue(Context& cx, Object* self, SlotIndex index, Value value) noexcept
{
	Value* pointer = (Value*)(self->fixedSlots_ + index.offset());
	*pointer       = value;
}

inline Shape*
Object::lookUpTransition(Context& cx, Infra::Span<const SlotAttr> attributes, std::size_t hash)
{
	return Om::lookUpTransition(cx, layout(), attributes, hash);
}

inline Shape*
Object::takeExistingTransition(
	Context& cx, Infra::Span<const SlotAttr> attributes, std::size_t hash)
{
	Shape* derivation = lookUpTransition(cx, attributes, hash);
	if (derivation != nullptr) {
		layout(derivation);
	}
	return derivation;
}

#if 0
inline Index Object::takeNewTransistion(Context& cx, Handle<Object> self, const SlotAttr& desc) {
  auto hash = desc.hash();
  return takeNewTransistion(cx, self, desc, hash);
}
#endif

inline Shape*
Object::takeNewTransition(
	Context& cx, Handle<Object> object, Infra::Span<const SlotAttr> attributes,
	std::size_t hash)
{
	RootRef<Shape> base(cx, object->layout());
	Shape* derivation = deriveObjectLayout(cx, base, attributes, hash);
	object->layout(derivation);
	return derivation;
	// TODO: Write barrier on objects taking a new transition
}

inline Shape*
Object::transition(Context& cx, Handle<Object> object, std::initializer_list<SlotAttr> list)
{
	return transition(cx, object, {list.begin(), list.size()});
}

inline Shape*
Object::transition(Context& cx, Handle<Object> object, Infra::Span<const SlotAttr> attributes)
{
	std::size_t hash = Om::hash(attributes);
	return transition(cx, object, attributes, hash);
}

inline Shape*
Object::transition(
	Context& cx, Handle<Object> object, Infra::Span<const SlotAttr> attributes,
	std::size_t hash)
{
	Shape* derivation = object->takeExistingTransition(cx, attributes, hash);
	if (derivation == nullptr) {
		derivation = takeNewTransition(cx, object, attributes, hash);
	}
	return derivation;
	// TODO: Write barrier on transitioning objects.
}

struct ObjectInitializer : public Initializer
{
	virtual Cell* operator()(Context& cx, Cell* cell) override
	{
		auto o = reinterpret_cast<Object*>(cell);
		o->layout(layout);
		o->fixedSlotCount_ = 32;
		return reinterpret_cast<Cell*>(o);
	}

	Handle<Shape> layout;
};

/// Allocate object with corresponding slot shape;
inline Object*
Object::allocate(Context& cx, Handle<Shape> layout)
{
	ObjectInitializer init;
	init.layout = layout;
	return BaseAllocator::allocate<Object>(cx, init);
}

inline Object*
allocateObject(Context& cx, Handle<Shape> layout) {
	return Object::allocate(cx, layout);
}

inline Object*
Object::allocate(Context& cx)
{
	RootRef<Shape> shape(cx, allocateRootObjectLayout(cx, {}));
	return allocate(cx, shape);
}

inline Object*
allocateEmptyObject(Context& cx) {
	return Object::allocate(cx);
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_OBJECT_INL_HPP_
