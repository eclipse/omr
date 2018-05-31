#if !defined(OMR_OM_CELLSLOTWALKER_HPP_)
#define OMR_OM_CELLSLOTWALKER_HPP_

#include <OMR/Om/dispatch.hpp>

namespace OMR {
namespace Om {

template <typename VisitorT>
struct VisitCell {
	VisitCell(VisitorT visitor)  : visitor(visitor) {}

	template <typename T>
	void operator()(T* thing) {
		thing->visit(visitor);
	}

	VisitorT visitor;
};

/// Visit the slots in a cell, based on the layout.
/// TODO: Implement in terms of dispatch, with a templated visit function.
template <typename VisitorT>
void dispatchVisit(Cell* cell, VisitorT visitor) {
	dispatch(cell, VisitCell<VisitorT>(visitor));
}

class CellSlotWalker {
public:
	template <typename VisitorT>
	void traverse(OMR::Om::Cell* cell, VisitorT visitor) {
		dispatchVisit(cell, visitor);
	}
};

}  // namespace Om
}  // namespace OMR

#endif // OMR_OM_CELLSLOTWALKER_HPP_
