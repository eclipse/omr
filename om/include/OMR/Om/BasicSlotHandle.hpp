#if !defined(OMR_OM_BASICSLOTHANDLE_HPP_)
#define OMR_OM_BASICSLOTHANDLE_HPP_

namespace OMR {
namespace Om {

/// A pointer into a object's slot.
class BasicSlotHandle {
public:
	template<typename T>
	explicit BasicSlotHandle(T** slot) : slot_(reinterpret_cast<Cell**>(slot)) {
		static_assert(sizeof(T*) == sizeof(Cell*),
		              "pointers must be a universally fixed width.");
		static_assert(sizeof(T**) == sizeof(Cell**),
		              "pointers must be a universally fixed width.");
	}

	void writeReference(Cell* ref) const { *slot_ = ref; }

	void atomicWriteReference(Cell* ref) const { assert(0); }

	Cell* readReference() const { return *slot_; }

private:
	Cell** slot_;
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_BASICSLOTHANDLE_HPP_
