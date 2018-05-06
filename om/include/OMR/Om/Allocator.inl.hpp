#ifndef OMR_OM_ALLOCATOR_INL_HPP_
#define OMR_OM_ALLOCATOR_INL_HPP_

#include <OMR/Om/Allocation.hpp>
#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/Handle.hpp>
#include <OMR/Om/Id.hpp>
#include <omrgc.h>

namespace OMR
{
namespace Om
{
template <typename ResultT, typename InitializerT>
inline ResultT*
BaseAllocator::allocate(Context& cx, InitializerT& init, std::size_t size)
{
	Allocation allocation(cx, init, size);
	ResultT* result = (ResultT*)allocation.allocateAndInitializeObject(cx.omrVmThread());

	if (DEBUG_SLOW) {
		RootRef<ResultT> root(cx, result);
		OMR_GC_SystemCollect(cx.omrVmThread(), 0);
		result = root.get();
	}

	return result;
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_ALLOCATOR_INL_HPP_
