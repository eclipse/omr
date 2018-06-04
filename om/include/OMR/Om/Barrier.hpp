#if !defined(OMR_OM_BARRIER_HPP_)
#define OMR_OM_BARRIER_HPP_

#include <OMR/Om/Context.hpp>

#include <StandardWriteBarrier.hpp>

namespace OMR {
namespace Om {

class Barrier {

	static void preWrite(Context& cx, void* cell, void* ref) {
		// Nothing yet.
	}

	static void postWrite(Context& cx, void* cell, void* ref) {
		standardWriteBarrier(cx.vmContext(), cell, ref);
	}

	/// Store to slot with pre/post GC barriers.
	template<typename SlotHandleT, typename T = void>
	static writeReference(Context& cx, void* cell, SlotHandleT slot, T* ref) {
		preWrite(cx, cell, slot.readReference());
		slot.writeReference(ref);
		postWrite(cx, cell, slot.readReference());
	}

	/// Store to slot atomically, with pre/post GC writebarriers.
	template<typename SlotHandleT, typename T = void>
	static atomicWriteReference(Context& cx, void* cell, SlotHandleT slot, T* ref) {
		slot.atomicWriteReference(ref);
		postWrite(cx, cell);
	}
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_BARRIER_HPP_
