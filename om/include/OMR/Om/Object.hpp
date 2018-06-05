#ifndef OMR_OM_OBJECT_HPP_
#define OMR_OM_OBJECT_HPP_

#include <OMR/Om/Array.hpp>
#include <OMR/Om/CellHeader.hpp>
#include <OMR/Om/Handle.hpp>
#include <OMR/Om/LayoutTree.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/SlotIndex.hpp>
#include <OMR/Om/Value.hpp>

#include <type_traits>

namespace OMR {
namespace Om {

class Context;

/// A Cell with dynamically allocated slots.
class Object {
public:
	static constexpr std::size_t MAX_SLOTS = 32;

	/// Calculate the allocation size of an object cell,
	static std::size_t cellSize(std::size_t inlineSlotSize) {
		return sizeof(Object) + inlineSlotSize;
	}

	CellHeader& header() { return header_; }

	const CellHeader& header() const { return header_; }

	Shape* layout() const { return header().layout(); }

	void layout(Shape* shape) { header().layout(shape); }

	/// Construct an iterable view of the tree of shapes that lay out this Object.
	LayoutTree layoutTree() const { return LayoutTree(layout()); }

	/// True if this object has no slots.
	bool empty() const {
		return (layout()->instanceSlotOffset() == 0)
		       && (layout()->instanceSlotCount() == 0);
	}

	/// Visit the slots in an object.
	/// TODO: Implement better slot iteration + Handle creation APIs
	/// TODO: Split into inline and outline slot walking.
	template<typename VisitorT>
	void visit(VisitorT& visitor) {
		visitHeader(header(), visitor);
		if (empty()) {
			return;
		}
		for (LayoutTree::Iterator layoutIter = layoutTree().begin();
		     layoutIter != layoutTree().end();
		     ++layoutIter) {
			Shape& shape = *layoutIter;
			SlotDescriptorRange slots = shape.instanceSlotDescriptorRange();

			for (SlotDescriptorRange::Iterator slotIter = slots.begin();
			     slotIter != slots.end();
			     slotIter++) {
				visitSlot(*slotIter, visitor);
			}
		}
	}

	template<typename VisitorT>
	void visitSlot(SlotDescriptor descriptor, VisitorT& visitor) {
		switch (descriptor.attr().type().coreType()) {
		case CoreType::VALUE: {
			Value* slot = reinterpret_cast<Value*>(inlineSlots_ + descriptor.offset());
			if (slot->isRef()) {
				ValueSlotHandle handle(slot);
				visitor.edge(this, handle);
			}
		} break;
		case CoreType::REF: {
			void** slot = reinterpret_cast<void**>(inlineSlots_ + descriptor.offset());
			visitor.edge(this, BasicSlotHandle(slot));
		} break;
		default: break;
		}
	}

	std::size_t inlineSlotSize() const { return layout()->instanceInlineSlotSize(); }

	/// The size of this cell of memory.
	std::size_t cellSize() const { return cellSize(inlineSlotSize()); }

	std::size_t totalMemoryFootprint() const { return cellSize() + overflowSlots_->cellSize(); }

	std::uint8_t* overflowSlots() {
		return reinterpret_cast<std::uint8_t*>(overflowSlots_->data());
	}

	std::uint8_t* inlineSlots() { return inlineSlots_; }

	bool isInlineSlot(SlotIndex index) { return index.offset() < inlineSlotSize(); }

	std::uint8_t* getInlineSlotAddress(SlotIndex index) {
		assert(isInlineSlot(index));
		return inlineSlots() + index.offset();
	}

	bool isOverflowSlot(SlotIndex index) { return !isInlineSlot(index); }

	std::uint8_t* getOverflowSlotAddress(SlotIndex index) {
		assert(overflowSlots_ != nullptr);
		assert(isOverflowSlot(index));
		return overflowSlots() + index.offset() - inlineSlotSize();
	}

	std::uint8_t* slotAddress(SlotIndex index) {
		if (isInlineSlot(index)) {
			return getInlineSlotAddress(index);
		} else {
			return getOverflowSlotAddress(index);
		}
	}

protected:
	friend struct ObjectInitializer;

	CellHeader header_;
	Array* overflowSlots_;
	std::uint8_t inlineSlots_[0];

private:
	static void construct(Context& cx, Handle<Object> self, Handle<Shape> shape);
};

static_assert(std::is_standard_layout<Object>::value, "Object must be a StandardLayoutType.");

} // namespace Om
} // namespace OMR

#endif // OMR_OM_OBJECT_HPP_
