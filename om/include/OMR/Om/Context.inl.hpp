#ifndef OMR_OM_CONTEXT_INL_HPP_
#define OMR_OM_CONTEXT_INL_HPP_

#include <OMR/Om/Context.hpp>
#include <OMR/Om/Runtime.hpp>

#include <EnvironmentBase.hpp>
#include <OMR_VMThread.hpp>
#include <mminitcore.h>
#include <omrutil.h>

namespace OMR {
namespace Om {

inline Context::Context(MemorySystem& system)
        : thread_(system.runtime().platform().thread()), manager_(&system), vmContext_(nullptr) {
	manager_->contexts().insert(this);
	attachVmContext(system.vm(), *this);
}

inline Context::~Context() noexcept {
	manager_->contexts().erase(this);
	detachVmContext(manager_->vm(), *this);
	vmContext_ = nullptr;
}

inline MM_EnvironmentBase* Context::gcContext() const noexcept {
	return MM_EnvironmentBase::getEnvironment(vmContext());
}

inline Context& getContext(MM_EnvironmentBase& gcContext) {
	return getContext(gcContext.getOmrVMThread());
}

inline Context& getContext(MM_EnvironmentBase* gcContext) { return getContext(*gcContext); }

} // namespace Om
} // namespace OMR

#endif // OM_CONTEXT_INL_HPP_
