#if !defined(OMR_OM_VALUESLOTHANDLE_HPP_)
#define OMR_OM_VALUESLOTHANDLE_HPP_

#include <OMR/Om/Value.hpp>

namespace OMR {
namespace Om {

/// A pointer into a object's slot.
class ValueSlotHandle
{
  public:

	constexpr ValueSlotHandle(Value* slot) : slot_(slot) {}

	void writeReference(Cell *ref) const { slot_->setRef(ref); }

	void atomicWriteReference(Cell *ref) const { assert(0); }

	Cell *readReference() const { return reinterpret_cast<Cell*>(slot_->getRef()); }

  private:
	Value *slot_;
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_VALUESLOTHANDLE_HPP_
