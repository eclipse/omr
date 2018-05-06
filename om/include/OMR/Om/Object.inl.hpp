#if !defined(OMR_OM_OBJECT_INL_HPP_)
#define OMR_OM_OBJECT_INL_HPP_

#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Object.hpp>

// #include <StandardWriteBarrier.hpp>

namespace OMR
{
namespace Om
{
#if 0  //////////////////////////////////////////

inline Object* Object::allocate(Context& cx, Handle<ObjectMap> map) {
  RootRef<Object> object = nullptr; // TODO: implement allocation
  construct(cx, object.reinterpret<Cell>(), map);
  return object.ptr();
}

inline void Object::construct(Context& cx, Handle<Object> self,
                              Handle<ObjectMap> map) {
  Cell::construct(cx, self, map);
  self->fixedSlots = fixedSlots;
  memset(slots_, 0, MAX_SLOTS * sizeof(Value));
  MemVector<Value>::construct(cx, {self, &Object::dynamicSlots}, 0);
}

inline void Object::clone(Context& xc, Handle<Object> base) {
  allocate(cx, base->map);
}

#endif  ////////////////////////////////////////////

/// TODO: Now that we support multiple CoreTypes, the result is not always the
/// same type. We should be returning the type of slot as well.
inline bool
Object::lookup(Context& cx, const Object* self, Id id, SlotDescriptor& result)
{
	for (const ObjectMap& map : self->mapHierarchy()) {
		for (const SlotDescriptor descriptor : map.slotDescriptors()) {
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

inline ObjectMap*
Object::lookUpTransition(Context& cx, Infra::Span<const SlotAttr> attributes, std::size_t hash)
{
	return map()->lookUpTransition(cx, attributes, hash);
}

#if 0
inline bool Object::takeExistingTransition(Context& cx, const ObjectDescription& desc, Index& result) {
  auto hash = desc.hash();
  return takeExistingTransition(desc, hash, result);
}
#endif

inline ObjectMap*
Object::takeExistingTransition(
	Context& cx, Infra::Span<const SlotAttr> attributes, std::size_t hash)
{
	ObjectMap* derivation = lookUpTransition(cx, attributes, hash);
	if (derivation != nullptr) {
		map(derivation);
		// TODO: Write barrier here?
	}
	return derivation;
}

#if 0
inline Index Object::takeNewTransistion(Context& cx, Handle<Object> self, const SlotAttr& desc) {
  auto hash = desc.hash();
  return takeNewTransistion(cx, self, desc, hash);
}
#endif

inline ObjectMap*
Object::takeNewTransition(
	Context& cx, Handle<Object> object, Infra::Span<const SlotAttr> attributes,
	std::size_t hash)
{
	RootRef<ObjectMap> base(cx, object->map());
	ObjectMap* derivation = ObjectMap::derive(cx, base, attributes, hash);
	object->map(derivation);
	return derivation;
	// TODO: Write barrier on objects taking a new transition
}

inline ObjectMap*
Object::transition(Context& cx, Handle<Object> object, std::initializer_list<SlotAttr> list)
{
	return transition(cx, object, {list.begin(), list.size()});
}

inline ObjectMap*
Object::transition(Context& cx, Handle<Object> object, Infra::Span<const SlotAttr> attributes)
{
	std::size_t hash = Om::hash(attributes);
	return transition(cx, object, attributes, hash);
}

inline ObjectMap*
Object::transition(
	Context& cx, Handle<Object> object, Infra::Span<const SlotAttr> attributes,
	std::size_t hash)
{
	ObjectMap* derivation = object->takeExistingTransition(cx, attributes, hash);
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
		o->map(map_);
		o->fixedSlotCount_ = 32;
		return &o->baseCell();
	}

	Handle<ObjectMap> map_;
};

/// Allocate object with corresponding slot map;
inline Object*
Object::allocate(Context& cx, Handle<ObjectMap> map)
{
	ObjectInitializer init;
	init.map_ = map.reinterpret<ObjectMap>();
	return BaseAllocator::allocate<Object>(cx, init);
}

inline Object*
Object::allocate(Context& cx)
{
	RootRef<ObjectMap> map(cx, ObjectMap::allocate(cx));
	return allocate(cx, map);
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_OBJECT_INL_HPP_
