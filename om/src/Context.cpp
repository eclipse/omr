#include <OMR/Om/MemorySystem.hpp>

#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/ShapeOperations.hpp>

#include <ParallelDispatcher.hpp>
#include "omrport.h"
#include "mminitcore.h"
#include "omr.h"
#include "omrvm.h"
#include "thread_api.h"
#include "omrutil.h"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "omrgcstartup.hpp"
#include "StartupManagerImpl.hpp"
#include "OMR_VMThread.hpp"

#include <iostream>
#include <cassert>

namespace OMR {
namespace Om {

/// Note:
void Context::attachVmContext(OMR_VM &vm, Context &cx)
{
	omrthread_t self;

	assert(nullptr == omr_vmthread_getCurrent(&vm));

	assert(omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT) == 0);
	omrthread_monitor_enter(vm._vmThreadListMutex);

	///////// FIRSTATTACH

	assert(omr_vmthread_firstAttach(&vm, &cx.vmContext_) == 0);
	setOMRVMThreadNameWithFlag(nullptr, cx.vmContext_, (char *)"OMR::Om::Context", TRUE);
	cx.vmContext_->_language_vmthread = &cx;
	assert(initializeMutatorModel(cx.vmContext_) == 0);
	omrthread_monitor_exit(vm._vmThreadListMutex);
	omrthread_detach(self);
}

void Context::detachVmContext(OMR_VM &vm, Context &cx)
{
	omrthread_t self;

	assert(cx.vmContext_->_attachCount == 1);
	// assert(nullptr != omr_vmthread_getCurrent(&vm));

	omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);

	/////// LASTFREE

	omrthread_monitor_enter(vm._vmThreadListMutex);
	cleanupMutatorModel(cx.vmContext_, TRUE);
	setOMRVMThreadNameWithFlag(nullptr, cx.vmContext_, NULL, FALSE);
	omr_vmthread_lastDetach(cx.vmContext_);
	omrthread_monitor_exit(vm._vmThreadListMutex);

	omrthread_detach(self);
}

}  // namespace Om
}  // namespace OMR
