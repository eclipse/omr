#ifndef OMR_OM_CONTEXT_INL_HPP_
#define OMR_OM_CONTEXT_INL_HPP_

#include <OMR/Om/Context.hpp>

#include <OMR/Om/Runtime.hpp>

#include <EnvironmentBase.hpp>
#include <OMR_VMThread.hpp>
#include <mminitcore.h>
#include <omrutil.h>

namespace OMR
{
namespace Om
{
inline Context::Context(MemoryManager& manager) : manager_(&manager)
{
	// auto e = OMR_Thread_Init(&manager.omrVm(), this, &omrVmThread_, "b9::Context");
	manager_->contexts().insert(this);
	// if (e != 0)
	// 	throw std::runtime_error("Failed to attach OMR thread to OMR VM");
}

inline Context::~Context() noexcept
{
	manager_->contexts().erase(this);
	// OMR_Thread_Free(omrVmThread_);
}

inline MM_EnvironmentBase*
Context::omrGcThread() const noexcept
{
	return MM_EnvironmentBase::getEnvironment(omrVmThread());
}

inline Context&
getContext(MM_EnvironmentBase& omrGcThread)
{
	return getContext(omrGcThread.getOmrVMThread());
}

inline Context&
getContext(MM_EnvironmentBase* omrGcThread)
{
	return getContext(*omrGcThread);
}

}  // namespace Om
}  // namespace OMR

#endif  // OM_CONTEXT_INL_HPP_
