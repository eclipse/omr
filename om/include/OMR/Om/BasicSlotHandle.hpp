#if !defined(OMR_OM_BASICSLOTHANDLE_HPP_)
#define OMR_OM_BASICSLOTHANDLE_HPP_

#include <cassert>

namespace OMR {
namespace Om {

class Cell;

/// A pointer into a object's slot.
class BasicSlotHandle {
public:
	template<typename T>
	BasicSlotHandle(T** slot) : slot_(reinterpret_cast<void**>(slot)) {
		static_assert(sizeof(T*) == sizeof(void*),
		              "pointers must be a universally fixed width.");
		static_assert(sizeof(T**) == sizeof(void**),
		              "pointers must be a universally fixed width.");
	}

	template <typename T = Cell>
	void writeReference(T* ref) const { *slot_ = reinterpret_cast<void*>(ref); }

	void atomicWriteReference(void* ref) const { assert(0); }

	template <typename T = Cell>
	T* readReference() const { return *reinterpret_cast<T**>(slot_); }

private:
	void** slot_;
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_BASICSLOTHANDLE_HPP_
