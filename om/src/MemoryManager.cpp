#include <OMR/Om/MemoryManager.inl.hpp>

#include <OMR/Om/Allocator.inl.hpp>
#include <OMR/Om/ArrayBufferShape.inl.hpp>
#include <OMR/Om/Context.inl.hpp>
#include <OMR/Om/MetaShape.inl.hpp>
#include <OMR/Om/RootRef.inl.hpp>
#include <OMR/Om/Traverse.hpp>

namespace OMR
{
namespace Om
{
void
Globals::init(StartupContext& cx)
{
	// The order of allocation matters. The MetaShape must be allocated before any
	// other map.
	metaMap_ = MetaShape::allocate(cx);

	// Tricky note: The object map's transition table is backed by an array
	// buffer. If the ArrayBufferShape isn't brought up first, than the first
	// empty object map allocated will not be able to allocate it's transition
	// table.
	arrayBufferMap_ = ArrayBufferShape::allocate(cx);
}

MemoryManager::MemoryManager(ProcessRuntime& runtime) : runtime_(runtime)
{
	Thread self(runtime.platform().thread());
	initOmrVm();
	initOmrGc();

	StartupContext cx(*this);
	OMR_GC_InitializeDispatcherThreads(cx.omrVmThread());

	globals_.init(cx);
}

MemoryManager::~MemoryManager()
{
	Thread self(runtime().platform().thread());
	// TODO: Shut down the heap (requires a thread (boo!!))
	omr_detach_vm_from_runtime(&omrVm());
}

void
MemoryManager::initOmrVm()
{
	memset(&omrVm_, 0, sizeof(OMR_VM));
	omrVm_._runtime             = &runtime_.omrRuntime();
	omrVm_._language_vm         = this;
	omrVm_._vmThreadList        = nullptr;
	omrVm_._gcOmrVMExtensions   = nullptr;
	omrVm_._languageThreadCount = 0;

	auto e = omr_attach_vm_to_runtime(&omrVm_);

	if (e != 0) {
		throw PlatformError(e);
	}
}

void
MemoryManager::initOmrGc()
{
	MM_StartupManagerImpl startupManager(&omrVm_);
	auto e = OMR_GC_IntializeHeapAndCollector(&omrVm_, &startupManager);
	if (e != 0) {
		throw PlatformError(e);
	}
}

}  // namespace Om
}  // namespace OMR
