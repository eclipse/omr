#ifndef OMR_OM_MAP_INL_HPP_
#define OMR_OM_MAP_INL_HPP_

#include <OMR/Om/Shape.hpp>

#include <OMR/Om/MetaShape.hpp>

namespace OMR
{
namespace Om
{
inline Shape::Shape(MetaShape* meta, Kind kind) : base_{&meta->baseShape()}, kind_(kind) {}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_MAP_INL_HPP_
