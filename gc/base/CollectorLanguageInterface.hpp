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
 ******************************************************************************/

#ifndef COLLECTORLANGUAGEINTERFACE_HPP_
#define COLLECTORLANGUAGEINTERFACE_HPP_

#include "BaseVirtual.hpp"
#include "modronbase.h"
#include "objectdescription.h"
#include "ConcurrentGCStats.hpp"

class GC_ObjectScanner;
class MM_Collector;
class MM_CompactScheme;
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
class MM_ConcurrentSafepointCallback;
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
class MM_EnvironmentBase;
class MM_EnvironmentStandard;
class MM_ForwardedHeader;
class MM_GlobalCollector;
class MM_MarkMap;
class MM_HeapRegionDescriptor;
class MM_HeapWalker;
class MM_MemorySubSpace;
class MM_MemorySubSpaceSemiSpace;
class MM_ParallelSweepScheme;
class MM_ScanClassesMode;
class MM_WorkPackets;

/**
 * Class representing a collector language interface. This defines the API between the OMR
 * functionality and the language being implemented.
 */
class MM_CollectorLanguageInterface : public MM_BaseVirtual {

private:
protected:
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	MM_ConcurrentGCStats _concurrentStats;
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
public:

private:
protected:
public:

	virtual void kill(MM_EnvironmentBase *env) = 0;

	/**
	 * Codes used to indicate why an object is being scanned
	 */
	enum MarkingSchemeScanReason {
		SCAN_REASON_PACKET = 1, /**< Indicates the object being scanned came from a work packet */
		SCAN_REASON_DIRTY_CARD = 2, /**< Indicates the object being scanned was found in a dirty card */
		SCAN_REASON_REMEMBERED_SET_SCAN = 3,
		SCAN_REASON_OVERFLOWED_OBJECT = 4, /**< Indicates the object being scanned was in an overflowed region */
	};

	virtual void doFrequentObjectAllocationSampling(MM_EnvironmentBase* env) = 0;
	virtual bool checkForExcessiveGC(MM_EnvironmentBase *env, MM_Collector *collector) = 0;
	virtual void flushNonAllocationCaches(MM_EnvironmentBase *env) = 0;
	virtual OMR_VMThread *attachVMThread(OMR_VM *omrVM, const char *threadName, uintptr_t reason) = 0;
	virtual void detachVMThread(OMR_VM *omrVM, OMR_VMThread *omrThread, uintptr_t reason) = 0;

	virtual bool globalCollector_isTimeForGlobalGCKickoff() = 0;
	virtual void globalCollector_internalPostCollect(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace) = 0;
	
	virtual void parallelGlobalGC_masterThreadGarbageCollect_beforeGC(MM_EnvironmentBase *env) = 0;
	virtual void parallelGlobalGC_masterThreadGarbageCollect_afterGC(MM_EnvironmentBase *env, bool compactThisCycle) = 0;
	virtual void parallelGlobalGC_postPrepareHeapForWalk(MM_EnvironmentBase *env) = 0;
	virtual void parallelGlobalGC_postMarkProcessing(MM_EnvironmentBase *env) = 0;
	virtual void parallelGlobalGC_setupBeforeGC(MM_EnvironmentBase *env) = 0;
	virtual void parallelGlobalGC_setMarkingScheme(MM_EnvironmentBase *env, void *markingScheme) = 0;
	virtual bool parallelGlobalGC_createAccessBarrier(MM_EnvironmentBase *env) = 0;
	virtual void parallelGlobalGC_destroyAccessBarrier(MM_EnvironmentBase *env) = 0;
	virtual bool parallelGlobalGC_heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress) = 0;
	virtual bool parallelGlobalGC_heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress) = 0;
	virtual bool parallelGlobalGC_createHeapWalker(MM_EnvironmentBase *env, MM_GlobalCollector *collector, MM_MarkMap *markMap) = 0;
	virtual void parallelGlobalGC_destroyHeapWalker(MM_EnvironmentBase *env) = 0;
	virtual MM_HeapWalker *parallelGlobalGC_getHeapWalker() = 0;
#if defined(OMR_GC_MODRON_COMPACTION)
	virtual CompactPreventedReason parallelGlobalGC_checkIfCompactionShouldBePrevented(MM_EnvironmentBase *env) = 0;
#endif /* OMR_GC_MODRON_COMPACTION */
	virtual void parallelGlobalGC_masterThreadGarbageCollect_gcComplete(MM_EnvironmentBase *env, bool didCompact) = 0;
	virtual void parallelGlobalGC_collectorInitialized(MM_EnvironmentBase *env) = 0;
	virtual void parallelGlobalGC_reportObjectEvents(MM_EnvironmentBase *env) = 0;
	
	virtual void markingScheme_masterSetupForGC(MM_EnvironmentBase *env) = 0;
	virtual void markingScheme_scanRoots(MM_EnvironmentBase *env) = 0;
	virtual void markingScheme_completeMarking(MM_EnvironmentBase *env) = 0;
	virtual void markingScheme_markLiveObjectsComplete(MM_EnvironmentBase *env) = 0;
	virtual void markingScheme_masterSetupForWalk(MM_EnvironmentBase *env) = 0;
	virtual void markingScheme_masterCleanupAfterGC(MM_EnvironmentBase *env) = 0;
	virtual uintptr_t markingScheme_scanObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MarkingSchemeScanReason reason) = 0;
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	virtual uintptr_t markingScheme_scanObjectWithSize(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MarkingSchemeScanReason reason, uintptr_t sizeToDo) = 0;
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
	/**
	 * This will be called for every allocated object.  Note this is not necessarily done when the object is allocated.  You are however
	 * guaranteed by the start of the next gc, you will be notified for all objects allocated since the last gc.
	 * hooktool is actually functionality better for this but is probably too heavy-weight for what we want for performant code.
	 */
	virtual bool objectAllocationNotify(MM_EnvironmentBase * env, omrobjectptr_t omrObject) { return true;} ;

	virtual omrobjectptr_t heapWalker_heapWalkerObjectSlotDo(omrobjectptr_t object) = 0;
	
	virtual void parallelDispatcher_handleMasterThread(OMR_VMThread *omrVMThread) = 0;

	virtual void workPacketOverflow_overflowItem(MM_EnvironmentBase *env, omrobjectptr_t objectPtr) = 0;
	
	virtual bool collectorHeapRegionDescriptorInitialize(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region) = 0;
	virtual void collectorHeapRegionDescriptorTearDown(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region) = 0;

#if defined(OMR_GC_MODRON_COMPACTION)
	virtual void compactScheme_languageMasterSetupForGC(MM_EnvironmentBase *env) = 0;
	virtual void compactScheme_fixupRoots(MM_EnvironmentBase *env, MM_CompactScheme *compactScheme) = 0;
	virtual void compactScheme_workerCleanupAfterGC(MM_EnvironmentBase *env) = 0;
	virtual void compactScheme_verifyHeap(MM_EnvironmentBase *env, MM_MarkMap *markMap) = 0;
#endif /* OMR_GC_MODRON_COMPACTION */

#if defined(OMR_GC_MODRON_SCAVENGER)
	virtual void scavenger_reportObjectEvents(MM_EnvironmentBase *env) = 0;
	virtual void scavenger_masterSetupForGC_clearLangStats(MM_EnvironmentBase * env) = 0;
	virtual void scavenger_workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase * envBase) = 0;
	virtual void scavenger_reportScavengeEnd(MM_EnvironmentBase * envBase, bool scavengeSuccessful) = 0;
	virtual void scavenger_mergeGCStats_mergeLangStats(MM_EnvironmentBase * envBase) = 0;
	virtual void scavenger_masterThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase * envBase) = 0;
	virtual void scavenger_masterThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *envBase) = 0;
	virtual bool scavenger_internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase * envBase, PercolateReason * percolateReason, uint32_t * gcCode) = 0;
	virtual GC_ObjectScanner *parallelScavenger_getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags) = 0;
	virtual void parallelScavenger_reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject) = 0;
#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	virtual void parallelScavenger_fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject, MM_MemorySubSpaceSemiSpace *subSpaceNew) = 0;
#endif /* OMR_INTERP_COMPRESSED_OBJECT_HEADER */
#endif /* OMR_GC_MODRON_SCAVENGER */

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	MMINLINE bool concurrentGC_hasConcurrentOverflowOcurred() { return _concurrentStats.getConcurrentWorkStackOverflowOcurred(); };
	MMINLINE bool concurrentGC_inConcurrentMarkCycle() { return (_concurrentStats.getExecutionMode() > CONCURRENT_OFF); };
	MM_ConcurrentGCStats *concurrentGC_getConcurrentStats() { return &_concurrentStats; };
	virtual void concurrentGC_processItem(MM_EnvironmentBase *env, omrobjectptr_t objectPtr) {};
	/* In the case of Weak Consistency platforms we require an additional API to bring mutator threads to a safe point */
	virtual MM_ConcurrentSafepointCallback* concurrentGC_createSafepointCallback(MM_EnvironmentBase *env) = 0;
	virtual uintptr_t concurrentGC_collectRoots(MM_EnvironmentStandard *env, ConcurrentStatus concurrentStatus, MM_ScanClassesMode *scanClassesMode, bool &collectedRoots, bool &paidTax) = 0;
	virtual omrsig_handler_fn concurrentGC_getProtectedSignalHandler(void *&signalHandlerArg) = 0;
	virtual void concurrentGC_signalThreadsToDirtyCards(MM_EnvironmentStandard *env) = 0;
	virtual void concurrentGC_signalThreadsToStopDirtyingCards(MM_EnvironmentStandard *env) = 0;
	virtual void concurrentGC_flushRegionReferenceLists(MM_EnvironmentBase *env) = 0;
	virtual void concurrentGC_flushThreadReferenceBuffer(MM_EnvironmentBase *env) = 0;
	virtual bool concurrentGC_isThreadReferenceBufferEmpty(MM_EnvironmentBase *env) = 0;
	virtual void concurrentGC_scanThread(MM_EnvironmentBase *env) = 0;
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

	MM_CollectorLanguageInterface()
		: MM_BaseVirtual()
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* COLLECTORLANGUAGEINTERFACE_HPP_ */
