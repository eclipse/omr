#if !defined(OMR_OM_DISPATCHVISIT_HPP_)
#define OMR_OM_DISPATCHVISIT_HPP_

#include <OMR/Om/dispatch.hpp>

#include <cassert>

namespace OMR {
namespace Om {

template <typename VisitorT>
struct Visit {
	Visit(VisitorT visitor)  : visitor(visitor) {}

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
	dispatch(cell, Visit<VisitorT>(visitor));
}

}  // namespace Om
}  // namespace OMR

#endif // OMR_OM_DISPATCHVISIT_HPP_
