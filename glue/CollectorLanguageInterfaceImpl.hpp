/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef COLLECTORLANGUAGEINTERFACEIMPL_HPP_
#define COLLECTORLANGUAGEINTERFACEIMPL_HPP_

#include "modronbase.h"
#include "omr.h"

#include "CollectorLanguageInterface.hpp"
#include "GCExtensionsBase.hpp"

class MM_CompactScheme;
class MM_EnvironmentStandard;
class MM_ForwardedHeader;
class MM_MarkingScheme;
class MM_MemorySubSpaceSemiSpace;
class MM_ScanClassesMode;

/**
 * Class representing a collector language interface.  This implements the API between the OMR
 * functionality and the language being implemented.
 * @ingroup GC_Base
 */
class MM_CollectorLanguageInterfaceImpl : public MM_CollectorLanguageInterface {
private:
protected:
	OMR_VM *_omrVM;
	MM_GCExtensionsBase *_extensions;
	MM_MarkingScheme *_markingScheme;
public:
	enum AttachVMThreadReason {
		ATTACH_THREAD = 0x0,
		ATTACH_GC_DISPATCHER_THREAD = 0x1,
		ATTACH_GC_HELPER_THREAD = 0x2,
		ATTACH_GC_MASTER_THREAD = 0x3,
	};

private:
protected:
	bool initialize(OMR_VM *omrVM);
	void tearDown(OMR_VM *omrVM);

	MM_CollectorLanguageInterfaceImpl(OMR_VM *omrVM)
		: MM_CollectorLanguageInterface()
		,_omrVM(omrVM)
		,_extensions(MM_GCExtensionsBase::getExtensions(omrVM))
	{
		_typeId = __FUNCTION__;
	}

public:
	static MM_CollectorLanguageInterfaceImpl *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	virtual void doFrequentObjectAllocationSampling(MM_EnvironmentBase* env) {}
	virtual bool checkForExcessiveGC(MM_EnvironmentBase *env, MM_Collector *collector) {return false;}

	virtual void flushNonAllocationCaches(MM_EnvironmentBase *env);

	virtual OMR_VMThread *attachVMThread(OMR_VM *omrVM, const char *threadName, uintptr_t reason);
	virtual void detachVMThread(OMR_VM *omrVM, OMR_VMThread *omrVMThread, uintptr_t reason);

	virtual bool globalCollector_isTimeForGlobalGCKickoff() {return false;}
	virtual void globalCollector_internalPostCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace) {}

	virtual void parallelGlobalGC_masterThreadGarbageCollect_beforeGC(MM_EnvironmentBase *env) {}
	virtual void parallelGlobalGC_masterThreadGarbageCollect_afterGC(MM_EnvironmentBase *env, bool compactThisCycle) {}
	virtual void parallelGlobalGC_postPrepareHeapForWalk(MM_EnvironmentBase *env) {}
	virtual void parallelGlobalGC_postMarkProcessing(MM_EnvironmentBase *env) {}
	virtual void parallelGlobalGC_setupBeforeGC(MM_EnvironmentBase *env) {}
	virtual void parallelGlobalGC_setMarkingScheme(MM_EnvironmentBase *env, void *markingScheme) {_markingScheme = (MM_MarkingScheme *)markingScheme;}
	virtual bool parallelGlobalGC_createAccessBarrier(MM_EnvironmentBase *env) {return true;}
	virtual void parallelGlobalGC_destroyAccessBarrier(MM_EnvironmentBase *env) {}
	virtual bool parallelGlobalGC_heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress) {return true;}
	virtual bool parallelGlobalGC_heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress) {return true;}
	virtual bool parallelGlobalGC_createHeapWalker(MM_EnvironmentBase *env, MM_GlobalCollector *collector, MM_MarkMap *markMap) {return true;}
	virtual void parallelGlobalGC_destroyHeapWalker(MM_EnvironmentBase *env) {}
	virtual MM_HeapWalker *parallelGlobalGC_getHeapWalker() {return NULL;}
	virtual void parallelGlobalGC_masterThreadGarbageCollect_gcComplete(MM_EnvironmentBase *env, bool didCompact) {}
	virtual void parallelGlobalGC_collectorInitialized(MM_EnvironmentBase *env) {}
	virtual void parallelGlobalGC_reportObjectEvents(MM_EnvironmentBase *env) {}

	virtual void markingScheme_masterSetupForGC(MM_EnvironmentBase *env);
	virtual void markingScheme_scanRoots(MM_EnvironmentBase *env);
	virtual void markingScheme_completeMarking(MM_EnvironmentBase *env);
	virtual void markingScheme_markLiveObjectsComplete(MM_EnvironmentBase *env);
	virtual void markingScheme_masterSetupForWalk(MM_EnvironmentBase *env);
	virtual void markingScheme_masterCleanupAfterGC(MM_EnvironmentBase *env);
	virtual uintptr_t markingScheme_scanObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MarkingSchemeScanReason reason);
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	virtual uintptr_t markingScheme_scanObjectWithSize(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MarkingSchemeScanReason reason, uintptr_t sizeToDo);
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

	virtual bool collectorHeapRegionDescriptorInitialize(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region) {return true;}
	virtual void collectorHeapRegionDescriptorTearDown(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region) {}

	virtual void parallelDispatcher_handleMasterThread(OMR_VMThread *omrVMThread);

	virtual void workPacketOverflow_overflowItem(MM_EnvironmentBase *env, omrobjectptr_t objectPtr) {}
	
	virtual omrobjectptr_t heapWalker_heapWalkerObjectSlotDo(omrobjectptr_t object);

#if defined(OMR_GC_MODRON_COMPACTION)
	virtual CompactPreventedReason parallelGlobalGC_checkIfCompactionShouldBePrevented(MM_EnvironmentBase *env) {return COMPACT_PREVENTED_NONE;}
	virtual void compactScheme_languageMasterSetupForGC(MM_EnvironmentBase *env);
	virtual void compactScheme_fixupRoots(MM_EnvironmentBase *env, MM_CompactScheme *compactScheme);
	virtual void compactScheme_workerCleanupAfterGC(MM_EnvironmentBase *env);
	virtual void compactScheme_verifyHeap(MM_EnvironmentBase *env, MM_MarkMap *markMap);
#endif /* OMR_GC_MODRON_COMPACTION */

#if defined(OMR_GC_MODRON_SCAVENGER)
	virtual void scavenger_reportObjectEvents(MM_EnvironmentBase *env);
	virtual void scavenger_masterSetupForGC_clearLangStats(MM_EnvironmentBase *env);
	virtual void scavenger_workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase *env);
	virtual void scavenger_reportScavengeEnd(MM_EnvironmentBase * envBase, bool scavengeSuccessful);
	virtual void scavenger_mergeGCStats_mergeLangStats(MM_EnvironmentBase *envBase);
	virtual void scavenger_masterThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase *envBase);
	virtual void scavenger_masterThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *envBase);
	virtual bool scavenger_internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase *envBase, PercolateReason *reason, uint32_t *gcCode);
	virtual GC_ObjectScanner *parallelScavenger_getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags);
	virtual void parallelScavenger_reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject) {}
#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	virtual void parallelScavenger_fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject, MM_MemorySubSpaceSemiSpace *subSpaceNew) {}
#endif /* OMR_INTERP_COMPRESSED_OBJECT_HEADER */
#endif /* OMR_GC_MODRON_SCAVENGER */

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	virtual MM_ConcurrentSafepointCallback* concurrentGC_createSafepointCallback(MM_EnvironmentBase *env);
	virtual uintptr_t concurrentGC_collectRoots(MM_EnvironmentStandard *env, ConcurrentStatus concurrentStatus, MM_ScanClassesMode *scanClassesMode, bool &collectedRoots, bool &paidTax);
	virtual omrsig_handler_fn concurrentGC_getProtectedSignalHandler(void *&signalHandlerArg) {return NULL;}
	virtual void concurrentGC_signalThreadsToDirtyCards(MM_EnvironmentStandard *env) {}
	virtual void concurrentGC_signalThreadsToStopDirtyingCards(MM_EnvironmentStandard *env) {}
	virtual void concurrentGC_flushRegionReferenceLists(MM_EnvironmentBase *env) {}
	virtual void concurrentGC_flushThreadReferenceBuffer(MM_EnvironmentBase *env) {}
	virtual bool concurrentGC_isThreadReferenceBufferEmpty(MM_EnvironmentBase *env) {return true;}
	virtual void concurrentGC_scanThread(MM_EnvironmentBase *env) {}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

};

#endif /* COLLECTORLANGUAGEINTERFACEIMPL_HPP_ */
