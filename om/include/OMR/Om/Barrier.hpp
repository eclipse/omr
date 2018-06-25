#if !defined(OMR_OM_BARRIER_HPP_)
#define OMR_OM_BARRIER_HPP_

#include <OMR/Om/Context.hpp>

// #include <StandardWriteBarrier.hpp>

namespace OMR {
namespace Om {

class BaseBarrier {
public:
	static void preWrite(Context& cx, void* cell, void* ref) {
		// Nothing yet.
	}

	static void postWrite(Context& cx, void* cell, void* ref) {
		// standardWriteBarrier(cx.vmContext(), cell, ref);
	}
};

template<typename SlotHandleT>
struct Barrier;

/// General read/write barriers for slots.
template<typename SlotHandleT>
struct Barrier : public BaseBarrier {
public:
	/// Store to slot with pre/post GC barriers.
	template<typename T = void*>
	static void writeReference(Context& cx, void* cell, SlotHandleT slot, T ref) {
		preWrite(cx, cell, slot.readReference());
		slot.writeReference(ref);
		postWrite(cx, cell, slot.readReference());
	}

	/// Store to slot atomically, with pre/post GC writebarriers.
	template<typename T = void*>
	static void atomicWriteReference(Context& cx, void* cell, SlotHandleT slot, T ref) {
		preWrite(cx, cell, slot.readReference());
		slot.atomicWriteReference(ref);
		postWrite(cx, cell, ref);
	}
};

/// Barrier specialization for Value slots. Specialization for value slot handles, where
/// the overwritten value may not be a reference.
template<>
struct Barrier<ValueSlotHandle> : public BaseBarrier {
public:
	/// Store to slot with pre/post GC barriers
	template<typename T = void*>
	static void writeReference(Context& cx, void* cell, ValueSlotHandle slot, T ref) {
		if (slot.isReference()) {
			preWrite(cx, cell, slot.readReference());
		}
		slot.writeReference(ref);
		postWrite(cx, cell, slot.readReference());
	}

	/// Store to slot atomically, with pre/post GC writebarriers.
	template<typename T = void*>
	static void atomicWriteReference(Context& cx, void* cell, ValueSlotHandle slot, T ref) {
		if (slot.isReference()) {
			preWrite(cx, cell, slot.readReference());
		}
		slot.atomicWriteReference(ref);
		postWrite(cx, cell, ref);
	}

	///
	/// Store to slot with pre/post GC barriers
	template<typename T = void*>
	static void write(Context& cx, void* cell, ValueSlotHandle slot, T ref) {
		if (slot.isReference()) {
			preWrite(cx, cell, slot.readReference());
		}
		slot.writeReference(ref);
		postWrite(cx, cell, slot.readReference());
	}
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_BARRIER_HPP_
