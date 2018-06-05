#if !defined(OMR_OM_VALUESLOTHANDLE_HPP_)
#define OMR_OM_VALUESLOTHANDLE_HPP_

#include <OMR/Om/Value.hpp>

namespace OMR {
namespace Om {

/// A pointer into a object's slot.
class ValueSlotHandle {
public:
	explicit constexpr ValueSlotHandle(Value* slot) : slot_(slot) {}

	explicit constexpr ValueSlotHandle(void* slot) : slot_((Value*)slot) {}

	void writeReference(void* ref) const { slot_->setRef(ref); }

	void writeReference(Value ref) const { *slot_ = ref; }

	void atomicWriteReference(void* ref) const { assert(0); }

	Cell* readReference() const { return reinterpret_cast<Cell*>(slot_->getRef()); }

private:
	Value* slot_;
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_VALUESLOTHANDLE_HPP_
