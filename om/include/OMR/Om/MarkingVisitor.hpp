#if !defined(OMR_GC_MARKINGVISITOR_HPP_)
#define OMR_GC_MARKINGVISITOR_HPP_

#include <ObjectVisitor.hpp>
#include <MarkingEnvironment.hpp>
#include <OMR/App/ObjectSlotHandle.hpp>

namespace OMR {
namespace GC {

class MarkingScheme;

/**
 * An object visitor that marks references for standard collection policies.
 */
class MarkingVisitor final : public ObjectVisitor
{
public:
	MarkingVisitor(GC::EnvironmentMetaData* metaData, GC::MarkingEnvironment* marking) : _metaData(metaData), _marking(marking) {}

	virtual void edge(omrobjectptr_t src, const ObjectSlotHandle* slot) override
	{
		_marking.markObject(slot->getObjectReference());
	}

	virtual void rootEdge(void* a, void* b) override
	{
		if (b != nullptr) {
			_marking.markObject(&_metaData, (Cell*)b);
		}
	}

	GC::EnvironmentMetaData* _metaData;
	GC::MarkingEnvironment* _marking;
};

}  /* namespace Om */
}  /* namespace OMR */

#endif  /* OMR_GC_MARKINGVISITOR_HPP_ */
