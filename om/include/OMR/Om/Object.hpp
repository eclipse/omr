#ifndef OMR_OM_OBJECT_HPP_
#define OMR_OM_OBJECT_HPP_

#include <OMR/Om/CellHeader.hpp>
#include <OMR/Om/Handle.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/Value.hpp>
#include <OMR/Om/LayoutTree.hpp>
#include <OMR/Om/SlotIndex.hpp>
#include <type_traits>

namespace OMR
{
namespace Om
{

class Context;

/// A Cell with dynamically allocated slots.
class Object
{
public:
	static constexpr std::size_t MAX_SLOTS = 32;

	/// Allocate object with this shape;
	static inline Object *allocate(Context &cx, Handle<Shape> shape);

	/// Allocate an empty object with a new EmptyObjectMap.
	static inline Object *allocate(Context &cx);

	static Object *clone(Context &cx, Handle<Object> base);

	// Take a layout/shape transition that hasn't been cached. Before taking a new
	// transition, users should use `lookupTransition` to ensure the transition
	// hasn't been taken before. See also `transition`, a higher level call for
	// transitioning across object layouts.
	static Shape *takeNewTransition(
		Context &cx, Handle<Object> object, Infra::Span<const SlotAttr> desc,
		std::size_t hash);

	/// Transition the object's shape by adding a set of new slots.
	/// This function will reuse cached transitions.
	static Shape *
	transition(Context &cx, Handle<Object> object, std::initializer_list<SlotAttr> slots);

	static Shape *
	transition(Context &cx, Handle<Object> object, Infra::Span<const SlotAttr> slots);

	/// Transition, but use a precomputer hash for the attributes.
	static Shape *transition(
		Context &cx, Handle<Object> object, Infra::Span<const SlotAttr> slots,
		std::size_t hash);

	static Value getValue(const Object *self, SlotIndex index) noexcept;

	/// Set the slot that corresponds to the id.
	static void setValue(Context &cx, Object *self, SlotIndex index, Value value) noexcept;

	static Cell* getRef(const Object *self, SlotIndex index) noexcept;

	/// Slot lookup by Id. The result is a SlotLookup, which describes the slot's
	/// offset and type.
	static bool lookup(Context &cx, const Object *self, Id id, SlotDescriptor &result);

	Shape *
	lookUpTransition(Context &cx, Infra::Span<const SlotAttr> desc, std::size_t hash);

	Shape *
	takeExistingTransition(Context &cx, Infra::Span<const SlotAttr> desc, std::size_t hash);

	CellHeader &header() { return header_; }

	const CellHeader &header() const { return header_; }

	Shape *layout() const { return header().layout(); }

	void layout(Shape *shape) { header().layout(shape); }

	/// Construct an iterable view of the tree of shapes that lay out this Object.
	LayoutTree layoutTree() const { return LayoutTree(layout()); }

	/// True if this object has no slots.
	bool empty() const { return (layout()->instanceSlotOffset() == 0) && (layout()->instanceSlotCount() == 0); }

	/// Visit the slots in an object.
	/// TODO: Implement better slot iteration + Handle creation APIs
	template <typename VisitorT>
	void visit(VisitorT &visitor)
	{
		visitHeader(header(), visitor);

		if (empty())
		{
			return;
		}

		for (LayoutTree::Iterator layoutIter = layoutTree().begin();
			 layoutIter != layoutTree().end();
			 ++layoutIter)
		{
			Shape &shape = *layoutIter;
			SlotDescriptorRange slots = shape.instanceSlotDescriptorRange();

			for (SlotDescriptorRange::Iterator slotIter = slots.begin();
				 slotIter != slots.end();
				 slotIter++)
			{
				SlotDescriptor descriptor = *slotIter;
				switch (descriptor.attr().type().coreType())
				{
				case CoreType::VALUE:
				{
					Value* slot = reinterpret_cast<Value*>(fixedSlots_ + descriptor.offset());
					if (slot->isRef()) {
						ValueSlotHandle handle(slot);
						visitor.edge(this, handle);
					}
				}
				break;
				case CoreType::REF:
				{
					void** slot = reinterpret_cast<void**>(fixedSlots_ + descriptor.offset());
					visitor.edge(this, BasicSlotHandle(slot));
				}
				break;
				default:
					break;
				}
			}
		}
	}

	/// The size of this cell of memory.
	std::size_t allocSize() const {
		/// TODO: Update when we support a variable number of fixed slots.
		return sizeof(Object);
	}

  protected:
	friend struct ObjectInitializer;

	CellHeader header_;

	// TODO: Dynamic slots in objects: MemVector<Value> dynamicSlots;
	// TODO: Variable number of fixed slots in objects
	std::size_t fixedSlotCount_;

	char fixedSlots_[sizeof(Value) * 32];

  private:
	static void construct(Context &cx, Handle<Object> self, Handle<Shape> shape);
};

static_assert(std::is_standard_layout<Object>::value, "Object must be a StandardLayoutType.");

} // namespace Om
} // namespace OMR

#endif // OMR_OM_OBJECT_HPP_
