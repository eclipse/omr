#if !defined(OMR_OM_DISPATCH_HPP_)
#define OMR_OM_DISPATCH_HPP_

#include <OMR/Om/Cell.hpp>
#include <OMR/Om/CellKind.hpp>
#include <OMR/Om/CellOperations.hpp>
#include <OMR/Om/Object.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/Array.hpp>

#include <cassert>

namespace OMR {
namespace Om {

/// Visit the slots in a cell, based on the layout.
template <typename FunctionT>
void dispatch(Cell* cell, FunctionT function) {
	switch(cellKind(cell)) {
	case CellKind::OBJECT: function(reinterpret_cast<Object*>(cell)); break;
	case CellKind::SHAPE: function(reinterpret_cast<Shape*>(cell)); break;
	case CellKind::ARRAY: function(reinterpret_cast<Array*>(cell)); break;
	default: assert(0);
	}
}

}  // namespace Om
}  // namespace OMR

#endif // OMR_OM_DISPATCH_HPP_
