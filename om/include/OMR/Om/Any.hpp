#if !defined(OMR_OM_ANY_HPP_)
#define OMR_OM_ANY_HPP_

#include <OMR/Om/Array.hpp>
#include <OMR/Om/Cell.hpp>
#include <OMR/Om/CellOperations.hpp>
#include <OMR/Om/Object.hpp>
#include <OMR/Om/Shape.hpp>

namespace OMR {
namespace Om {

union Any {
	Cell cell;
	Object object;
	Shape shape;
	Array array;
};

/// @group Casting and Reinterpreting Any types.
/// @{

inline Cell* asCell(Any* any) { return &any->cell; }

inline const Cell* asCell(const Any* any) { return &any->cell; }

inline Object* asObject(Any* any) { return &any->object; }

inline const Object* asObject(Any* any) { return &any->object; }

inline Shape* asShape(const Any* any) { return &any->shape; }

inline const Shape* asShape(const Any* any) { return &any->shape; }

/// @}

inline CellKind cellKind(const Any& any) const { return cellKind(asCell(any)); }

inline CellKind cellKind(const Any* any) const { return cellKind(*any); }

} // namespace Om
} // namespace OMR

#endif // OMR_OM_ANY_HPP_
