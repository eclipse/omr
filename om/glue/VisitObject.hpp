#if !defined(OMRAPP_VISITOBJECT_HPP_)
#define OMRAPP_VISITOBJECT_HPP_

#include <OMR/Om/dispatchVisit.hpp>

namespace OMRApp {

template <typename VisitorT>
struct VisitObject {
	void operator()(OMR::Om::Cell* cell, VisitorT visitor) {
		OMR::Om::dispatchVisit(cell, visitor);
	}
};

} // namespace OMRApp

#endif // OMRAPP_VISITOBJECT_HPP_
