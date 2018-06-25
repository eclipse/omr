#if !defined(OMR_OM_ANYSLOTWALKER_HPP_)
#define OMR_OM_ANYSLOTWALKER_HPP_

#include <OMR/Om/ObjectOperations.hpp>
#include <OMR/Om/LayoutTree.hpp>

namespace OMR {
namespace Om {

#if 0
class MarkingVisitor {
public:
	template <typename SlotHandleT>
	visitEdge(SlotHandleT slot) {
		mark(slot.readReference());
		return true;
	}
};
#endif

class BaseSlotWalker {
public:
	Cell* cell_;
};

class ShapeWalker {
public:
	enum class Phase {
		LAYOUT, PARENT_LAYOUT, TRANSITION_TABLE
	};

	ShapeWalker(Cell* cell) {}

	template <typename VisitorT>
	bool walk(VisitorT visitor) {}
};

class ObjectSlotWalker {
public:
	enum class Phase {
		HEADER, OVERFLOW_SLOTS_BUFFER_REF, SLOTS, DONE
	};

	ObjectSlotWalker(Object* object) : object_(object), phase_(Phase::HEADER), continue_(false) {}

	// returns true on completion, false on interruption.
	template <typename VisitorT>
	bool traverse(VisitorT visitor) {
		continue_ = true;
		while (continue_) {
			switch(phase_) {
			case Phase::HEADER:
				scanHeader(visitor);
				break;
			case Phase::OVERFLOW_SLOTS_BUFFER_REF:
				scanOverflowSlotsBufferRef(visitor);
				break;
			case Phase::SLOTS:
				scanSlots(visitor);
				break;
			case Phase::DONE:
				return true;
			}
		}
		return false;
	}

private:
	template <typename VisitorT>
	void scanHeader(VisitorT visitor) {
		continue_ = visitor.edge(object_, CellHeaderLayoutHandle(&object_->header()));
		// next: the reference to the overflow slots buffer
		phase_ = Phase::OVERFLOW_SLOTS_BUFFER_REF;
	}

	template <typename VisitorT>
	void scanOverflowSlotsBufferRef(VisitorT visitor) {
		continue_ = visitor.edge(object_, BasicSlotHandle(&object_->overflowSlots_));
		// next: the slots
		layoutIter_ = object_->layoutTree().begin();
		Shape& shape = *layoutIter_;
		slotIter_ = shape.instanceSlotDescriptorRange().begin();
		phase_ = Phase::SLOTS;
	}

	template <typename VisitorT>
	void scanSlots(VisitorT visitor) {
		while (layoutIter_ != object_->layoutTree().end()) {
			Shape& shape = *layoutIter_;
			SlotDescriptorRange slotRange = shape.instanceSlotDescriptorRange();
			SlotDescriptorRangeIterator end = slotRange.end();
			while (slotIter_ != end) {
				continue_ = object_->visitSlotB(*slotIter_, visitor);
				++slotIter_;
				if (!continue_) {
					return;
				}
			}
			++layoutIter_;
			if (layoutIter_ != object_->layoutTree().end()) {
				slotIter_ = (*layoutIter_).instanceSlotDescriptorRange().begin();
			}
		}
		// next: finished
		phase_ = Phase::DONE;
	}

	Object* object_;
	LayoutTree::Iterator layoutIter_;
	SlotDescriptorRange::Iterator slotIter_;
	Phase phase_;
	bool continue_;
};

/// Slot walker for any cell type.
class AnySlotWalker {
public:

	AnySlotWalker(Cell* cell) : subwalker_(cell) {
		switch(cellKind(subwalker_.cell)) {
		case CellKind::OBJECT: new(&subwalker_.object) ObjectSlotWalker((Object*)cell); break;
		default:
			assert(0);
		}
	}

	template <typename VisitorT>
	bool traverse(VisitorT visitor) {
		switch(cellKind(subwalker_.cell)) {
		case CellKind::OBJECT: return subwalker_.object.traverse(visitor);
		// case CellKind::SHAPE: return subwalker_.shape.traverse(visitor);
		default:
			assert(0);
			return true;
		}
	}

private:
	union SubWalkers {
		SubWalkers(Cell* c) : cell(c) {}
		Cell* cell;
		BaseSlotWalker base;
		ObjectSlotWalker object;
	} subwalker_;
};

}  // namespace Om
}  // namespace OMR

#endif // OMR_OM_ANYSLOTWALKER_HPP_
