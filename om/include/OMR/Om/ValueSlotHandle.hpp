#if !defined(OMR_OM_VALUESLOTHANDLE_HPP_)
#define OMR_OM_VALUESLOTHANDLE_HPP_

#include <OMR/Om/Value.hpp>

namespace OMR {
namespace Om {

/// A handle to an object's slot. Slot constains a Value.
class ValueSlotHandle {
public:
	explicit constexpr ValueSlotHandle(Value* slot) : slot_(slot) {}

	explicit constexpr ValueSlotHandle(void* slot) : slot_((Value*)slot) {}

	void writeReference(void* ref) const { slot_->setRef(ref); }

	void writeReference(Value ref) const { *slot_ = ref; }

	void atomicWriteReference(void* ref) const { assert(0); }

	template<typename T = Cell>
	T* readReference() const {
		return slot_->getRef<T>();
	}

	bool isReference() const { return slot_->isRef(); }

	Value* slot() const { return slot_; }

private:
	Value* slot_;
};

/// A Handle to an object's slot. Slot contains an immutable Value.
class ConstValueSlotHandle {
public:
	ConstValueSlotHandle(ValueSlotHandle& other) : slot_(other.slot()) {}

	explicit constexpr ConstValueSlotHandle(const Value* slot) : slot_(slot) {}

	explicit constexpr ConstValueSlotHandle(const void* slot) : slot_((Value*)slot) {}

	template<typename T = Cell>
	T* readReference() const {
		return slot_->getRef<T>();
	}

	bool isReference() const { return slot_->isRef(); }

	const Value* slot() const { return slot_; }

private:
	const Value* slot_;
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_VALUESLOTHANDLE_HPP_
