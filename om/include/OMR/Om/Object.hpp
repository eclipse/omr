#ifndef OMR_OM_OBJECT_HPP_
#define OMR_OM_OBJECT_HPP_

#include <OMR/Om/Cell.hpp>
#include <OMR/Om/Handle.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/ObjectMap.hpp>
#include <OMR/Om/Value.hpp>

#include <type_traits>

namespace OMR
{
namespace Om
{
class Context;

/// An adapter class that provides macro operations on the map hierachy of an
/// object. Most notably, the ObjectMapHierarchy is iterable.
class ObjectMapHierachy
{
public:
	class Iterator
	{
	public:
		explicit constexpr Iterator(ObjectMap* start) noexcept : current_(start) {}

		Iterator(const Iterator&) = default;

		Iterator operator++(int) noexcept
		{
			Iterator copy(*this);
			current_ = current_->parent();
			return copy;
		}

		Iterator& operator++() noexcept
		{
			current_ = current_->parent();
			return *this;
		}

		constexpr ObjectMap& operator*() const noexcept { return *current_; }

		constexpr bool operator==(const Iterator& rhs) const noexcept
		{
			return current_ == rhs.current_;
		}

		constexpr bool operator!=(const Iterator& rhs) const noexcept
		{
			return current_ != rhs.current_;
		}

	protected:
		friend class ObjectMapHierachy;

	private:
		ObjectMap* current_;
	};

	constexpr ObjectMapHierachy(ObjectMap* start) : start_(start) {}

	Iterator begin() const noexcept { return Iterator(start_); }

	Iterator end() const noexcept { return Iterator(nullptr); }

private:
	ObjectMap* start_;
};

struct ObjectDeclaration
{
};

/// A Cell with dynamically allocated slots.
struct Object
{
	union Base
	{
		Cell cell;
	};

	static constexpr std::size_t MAX_SLOTS = 32;

	/// Allocate object with this map;
	static inline Object* allocate(Context& cx, Handle<ObjectMap> map);

	/// Allocate an empty object with a new EmptyObjectMap.
	static inline Object* allocate(Context& cx);

	static Object* clone(Context& cx, Handle<Object> base);

	// Take a layout/map transition that hasn't been cached. Before taking a new
	// transition, users should use `lookupTransition` to ensure the transition
	// hasn't been taken before. See also `transition`, a higher level call for
	// transitioning across object layouts.
	static ObjectMap* takeNewTransition(
		Context& cx, Handle<Object> object, Infra::Span<const SlotAttr> desc,
		std::size_t hash);

	/// Transition the object's shape by adding a set of new slots.
	/// This function will reuse cached transitions.
	static ObjectMap*
	transition(Context& cx, Handle<Object> object, std::initializer_list<SlotAttr> slots);

	static ObjectMap*
	transition(Context& cx, Handle<Object> object, Infra::Span<const SlotAttr> slots);

	/// Transition, but use a precomputer hash for the attributes.
	static ObjectMap* transition(
		Context& cx, Handle<Object> object, Infra::Span<const SlotAttr> slots,
		std::size_t hash);

	static Value getValue(const Object* self, SlotIndex index) noexcept;

	/// Set the slot that corresponds to the id.
	static void setValue(Context& cx, Object* self, SlotIndex index, Value value) noexcept;

	/// Slot lookup by Id. The result is a SlotLookup, which describes the slot's
	/// offset and type.
	static bool lookup(Context& cx, const Object* self, Id id, SlotDescriptor& result);

	ObjectMap*
	lookUpTransition(Context& cx, Infra::Span<const SlotAttr> desc, std::size_t hash);

	ObjectMap*
	takeExistingTransition(Context& cx, Infra::Span<const SlotAttr> desc, std::size_t hash);

	Base& base() { return base_; }

	const Base& base() const { return base_; }

	Cell& baseCell() { return base().cell; }

	const Cell& baseCell() const { return base().cell; }

	ObjectMap* map() const { return reinterpret_cast<ObjectMap*>(baseCell().map()); }

	void map(ObjectMap* m) { baseCell().map(&m->baseShape()); }

	ObjectMapHierachy mapHierarchy() const { return ObjectMapHierachy(map()); }

	/// True if this object has no slots.
	bool empty() const { return (map()->slotOffset() == 0) && (map()->slotCount() == 0); }

	template <typename VisitorT>
	void visit(VisitorT& visitor)
	{
		baseCell().visit(visitor);

		if (empty()) {
			return;
		}

		for (MapHierachyIterator iter = mapHierarchy().begin(); iter != mapHierachy().end(); ++iter) {
			
		}
			for (const SlotDescriptor descriptor : map.slotDescriptors()) {
				switch (descriptor.attr().type().coreType()) {
				case CoreType::VALUE: {
					Value value = getValue(this, descriptor);
					if (value.isPtr()) {
						visitor.edge((Cell*)this, value.getPtr<Cell>());
					}
				} break;
				default:
					// TODO: Support more CoreTypes in objects.
					assert(0);
				}
			}
		}
	}

protected:
	friend struct ObjectInitializer;

	Base base_;
	// TODO: Dynamic slots in objects: MemVector<Value> dynamicSlots;
	// TODO: Variable number of fixed slots in objects
	std::size_t fixedSlotCount_;
	char fixedSlots_[sizeof(Value) * 32];

private:
	static void construct(Context& cx, Handle<Object> self, Handle<ObjectMap> map);
};

static_assert(std::is_standard_layout<Object>::value, "Object must be a StandardLayoutType.");

}  // namespace Om
}  // namespace OMR

#endif  // B9_OBJECTS_HPP_
