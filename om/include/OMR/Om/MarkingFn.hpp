#if !defined(OMR_OM_MARKINGFN_HPP_)
#define OMR_OM_MARKINGFN_HPP_

#include <functional>
#include <vector>

namespace OMR
{
namespace Om
{

class Visitor;

using MarkingFn = std::function<void(Visitor&)>;

using MarkingFnVector = std::vector<MarkingFn>;

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_MARKINGFN_HPP_
