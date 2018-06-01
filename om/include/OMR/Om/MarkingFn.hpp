#if !defined(OMR_OM_MARKINGFN_HPP_)
#define OMR_OM_MARKINGFN_HPP_

#include <MarkingScheme.hpp>
#include <functional>
#include <vector>

namespace OMR {
namespace Om {

class Visitor;

using MarkingVisitor = MM_MarkingScheme::MarkingVisitor;

using MarkingFn = std::function<void(MarkingVisitor&)>;

using MarkingFnVector = std::vector<MarkingFn>;

} // namespace Om
} // namespace OMR

#endif // OMR_OM_MARKINGFN_HPP_
