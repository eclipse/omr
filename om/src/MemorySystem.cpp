#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Context.inl.hpp>
#include <OMR/Om/MemorySystem.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/ShapeOperations.hpp>

#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "OMR_VMThread.hpp"
#include "StartupManagerImpl.hpp"
#include "mminitcore.h"
#include "omr.h"
#include "omrgcstartup.hpp"
#include "omrport.h"
#include "omrutil.h"
#include "omrvm.h"
#include "thread_api.h"
#include <ParallelDispatcher.hpp>
#include <iostream>

namespace OMR {
namespace Om {

void Globals::init(StartupContext& cx) {
	// The order of allocation matters. The MetaShape must be allocated before any
	// other shape.
	metaShape_ = allocateShapeLayout(cx);

	// Tricky note: The object shape's transition table is backed by an array
	// buffer. If the ArrayBufferShape isn't brought up first, than the first
	// empty object shape allocated will not be able to allocate it's transition
	// table.
	arrayBufferShape_ = allocateArrayLayout(cx);
}

MemorySystem::MemorySystem(ProcessRuntime& runtime) : runtime_(runtime) {
	std::cerr << __PRETTY_FUNCTION__ << std::endl;

	Thread self(runtime.platform().thread());
	initOmrVm();
	initOmrGc();

	StartupContext cx(*this);
	initOmrGcSlaveThreads(cx);
	globals_.init(cx);
}

MemorySystem::~MemorySystem() {
	Thread self(runtime().platform().thread());
	// TODO: Shut down the heap (requires a thread (boo!!))
	omr_detach_vm_from_runtime(&vm());
}

void MemorySystem::initOmrVm() {
	memset(&omrVm_, 0, sizeof(OMR_VM));
	omrVm_._runtime = &runtime_.omrRuntime();
	omrVm_._language_vm = this;
	omrVm_._vmThreadList = nullptr;
	omrVm_._gcOmrVMExtensions = nullptr;
	omrVm_._languageThreadCount = 0;

	auto e = omr_attach_vm_to_runtime(&omrVm_);

	if (e != 0) {
		throw PlatformError(e);
	}
}

void MemorySystem::initOmrGc() {
	MM_StartupManagerImpl startupManager(&omrVm_);
	auto e = OMR_GC_IntializeHeapAndCollector(&omrVm_, &startupManager);
	if (e != 0) {
		throw PlatformError(e);
	}
}

void MemorySystem::initOmrGcSlaveThreads(StartupContext& cx) {
	auto extensions = cx.gcContext()->getExtensions();
	assert(extensions != nullptr);
	auto ok = extensions->dispatcher->startUpThreads();
	if (!ok) {
		extensions->dispatcher->shutDownThreads();
		throw PlatformError(0);
	}
}

} // namespace Om
} // namespace OMR
