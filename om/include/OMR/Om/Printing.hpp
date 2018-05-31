#if !defined(OMR_OM_PRINTING_HPP_)
#define OMR_OM_PRINTING_HPP_

#include <OMR/Om/BasicSlotHandle.hpp>
#include <OMR/Om/Cell.hpp>
#include <OMR/Om/ValueSlotHandle.hpp>

#include <ostream>

namespace OMR {
namespace Om {

/// A debug helper for printing the slots in an object.

operator<<(std::ostream& out, BasicSlotHandle)
class PrintingVisitor {
public:

	template <typename SlotHandleT>
	void edgeSlot(void* cell, SlotHandleT slotHandle) {
		out_ << 
	}

private:
	std::ostream& out_;
};

} // 

operator<<(std::ostream& out, Cell* cell) {
	dispatchVisit(cell, )
}

}
}

#endif // OMR_OM_PRINTINGVISITOR_HPP_
