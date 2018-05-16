#if !defined(OMR_CELLOPERATIONS_HPP_)
#define OMR_CELLOPERATIONS_HPP_

#include <OMR/Om/Cell.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/CellKind.hpp>

namespace OMR {
namespace Om {

/// Obtain the type of thing this cell is. The type can be found by looking at the instance data in the Shape that lays out this Cell.
CellKind cellKind(const Cell* cell) {
	return cell->layout()->instanceKind();
}

} // namespace Om
} // namespace OMR

#endif // OMR_CELLOPERATIONS_HPP_
