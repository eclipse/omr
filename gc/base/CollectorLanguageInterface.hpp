/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef COLLECTORLANGUAGEINTERFACE_HPP_
#define COLLECTORLANGUAGEINTERFACE_HPP_

#include "modronbase.h"
#include "objectdescription.h"

#include "BaseVirtual.hpp"
#include "ConcurrentGCStats.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "SlotObject.hpp"

class GC_ObjectScanner;
class MM_Collector;
class MM_CompactScheme;
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
class MM_ConcurrentSafepointCallback;
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
class MM_EnvironmentStandard;
class MM_ForwardedHeader;
class MM_GlobalCollector;
class MM_MarkMap;
class MM_HeapRegionDescriptor;
class MM_HeapWalker;
class MM_MemorySubSpace;
class MM_MemorySubSpaceSemiSpace;
class MM_ParallelSweepScheme;
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

#if defined(OMR_GC_MODRON_COMPACTION)
	virtual void compactScheme_languageMasterSetupForGC(MM_EnvironmentBase *env) = 0;
	virtual void compactScheme_fixupRoots(MM_EnvironmentBase *env, MM_CompactScheme *compactScheme) = 0;
	virtual void compactScheme_workerCleanupAfterGC(MM_EnvironmentBase *env) = 0;
	virtual void compactScheme_verifyHeap(MM_EnvironmentBase *env, MM_MarkMap *markMap) = 0;
#endif /* OMR_GC_MODRON_COMPACTION */

#if defined(OMR_GC_MODRON_SCAVENGER)
	/**
	 * This method will be called on the master GC thread after each scavenger cycle, successful or
	 * otherwise. It may be used for reporting or other tasks as required.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] scavengeSuccessful Indicates whether the scavenge cycle completed normally or aborted.
	 */
	virtual void scavenger_reportScavengeEnd(MM_EnvironmentBase * env, bool scavengeSuccessful) = 0;

	/**
	 * This method is called on the master GC thread when a scavenge cycle is started. Implementations of
	 * this method may clear any client-side stats or metadata relating to scavenge cycles at this point.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void scavenger_masterSetupForGC(MM_EnvironmentBase * env) = 0;

	/**
	 * This method is called on each GC worker thread when a scavenge cycle is started. Implementations of
	 * this method may clear any thread-specific client-side stats or metadata relating to scavenge cycles
	 * at this point.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void scavenger_workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase * env) = 0;

	/**
	 * This method is called on each GC worker thread when it completes its participation in a scavenge
	 * cycle. It may be used to merge stats collected on each worker thread into global stats.
	 */
	virtual void scavenger_mergeGCStats_mergeLangStats(MM_EnvironmentBase * env) = 0;

	/**
	 * This method is called on the master GC thread when a scavenge cycle completes, successfully or otherwise.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void scavenger_masterThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase * env) = 0;

	/**
	 * This method is called on the master GC thread when a scavenge cycle completes successfully.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void scavenger_masterThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *env) = 0;

	/**
	 * If a scavenge cycle is unable to start or complete successfully, the GC may be "percolated" up
	 * to a global GC cycle. This method is called before the decision to percolate is made. If this method
	 * returns true, percolation will be attempted. In that case, the implementation must specify and
	 * return a PercolateReason and a gc code.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[out] percolateReason Reason for percolation
	 * @param[out] gcCode GC code to use for global collection
	 * @return true if GC should percolate to global collection
	 */
	virtual bool scavenger_internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase * env, PercolateReason * percolateReason, uint32_t * gcCode) = 0;

	/**
	 * An object scanner is required to locate and scan all object references associated with each root object
	 * and every object that is evacuated from allocate space during a scavenge cycle. this method must instantiate
	 * in the allocSpace provided an object scanner instance that is appropriate for the object presented. The
	 * allocSpace is guaranteed to be large enough to hold an instance of the largest GC_ObjectScanner subclass
	 * represented in the GC_ObjectScannerState structure.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] objectPtr The object to be scanned
	 * @param[in] allocSpace Space for in-place instantiation of scanner
	 * @param[in] flags See GC_ObjectScanner::InstanceFlags. One of scanRoots or scanHeap will be set , but not both.
	 * @return Pointer to object scanner, or NULL if object not to be scanned (eg, leaf object).
	 * @see GC_ObjectScanner
	 */
	virtual GC_ObjectScanner *scavenger_getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags) = 0;

	/**
	 * Scavenger calls this method when required to force GC threads to flush any locally-held references into
	 * associated global buffers.
	 * 
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void scavenger_flushReferenceObjects(MM_EnvironmentStandard *env) = 0;
	
	/**
	 * An indirect referent is a refernce to an object in the heap that is not a root object and is not reachable from
	 * any root object, for example, a Java Class object. This method is called to determine whether and object is associated
	 * with any indirect objects that are currently in new space.
	 * 
	 * @param[in] env The environment for the calling thread.
	 * @param[in] objectPtr Reference to the object that may have associated indirect referents.
	 * @return true if the object has associated indirect referents.
	 */
	virtual bool scavenger_hasIndirectReferentsInNewSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr) = 0;

	/**
	 * This method will be called for any object that has indirect object references. The implementation must call
	 * MM_Scavenger::copyObjectSlot(env, slot) for each reference slot in each indirect object associated with the
	 * object. The object will be included in the remembered set if the method returns true.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] objectPtr Reference to the object that has associated indirect referents.
	 * @return true if MM_Scavenger::copyObjectSlot(env, slot) returns true for any slot.
	 */
	virtual bool scavenger_scavengeIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr) = 0;

	/**
	 * If scavenger is unable to complete a cycle eg, because there is insufficient tenure space remaining to promote
	 * an object from new space to tenure space), it must back out of the scavenge cycle and percolate the collection.
	 * In that context, this method will be called for each root object and for each object reachable from a root
	 * object. Implementation must identify all indirect objects associated with the root or reachable object and call
	 * MM_Scavenger::backOutFixSlotWithoutCompression(slot) for each reference slot in each indirect object.
	 *
	 * Backout is a single-threaded operation at present.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] objectPtr Reference to an object that may have associated indirect referents.
	 */
	virtual void scavenger_backOutIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr) = 0;

	/* This method must be implemented if an object may hold any object references that are live but not reachable
	 * by traversing the reference graph from the root set or remembered set. In that case, this method should locate
	 * all such objects and call MM_Scavenger::backOutObjectScan(..) for each such object that is in the remembered
	 * set. For example,
	 *
	 * if (_extensions->objectModel.isRemembered(indirectObject)) {
	 *    _extensions->scavenger->backOutObjectScan(env, indirectObject);
	 * }
	 *
	 * This method is called only in backout contexts, and only when the remembered set is in an overflow state.
	 *
	 * Backout is a single-threaded operation at present.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void scavenger_backOutIndirectObjects(MM_EnvironmentStandard *env) = 0;

	/**
	 * This method is called during backout while undoing an object copy operation. This may involve restoring the scavenger
	 * forwarding slot, which may have been overwritten in the original copy process, and backing out changes to object
	 * flags in the forwarding slot. With the exception of the scavenger forwarding slot, all of the other bits of the
	 * original copied object should be intact.
	 *
	 * At minimum, if compressed pointers are enabled, this method must call MM_ForwardedHeader::restoreDestroyedOverlap()
	 * to restore the (4-byte) slot overlapping the (8-byte) scavenger forwarding slot.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] forwardedObject A forwarded header containing the original and destination addresses of the object.
	 */
	virtual void scavenger_reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject) = 0;

#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	/**
	 * This method is similar to scavenger_reverseForwardedObject() but is called from a different context. The implementation
	 * must first determine whether or not the compressed slot adjacent to the scavenger forwarding slot may contain an object
	 * reference. In that case, this implementation must:
	 *
	 * 1- get the preserved adjacent slot value from the forwarded header (originalForwardedHeader->getPreservedOverlap())
	 * 2- expand the preserved adjacent slot value to obtain an uncompressed pointer P to the referenced object (see GC_SlotObject)
	 * 3- test whether the referenced object has object alignment and is in new space or tenure space (stop otherwise)
	 * 4- instantiate a new MM_ForwardedHeader instance F using the referenced object pointer
	 * 5- test whether F is a reverse forwarded object (F.isReverseForwardedPointer(), stop otherwise)
	 * 6- call originalForwardedHeader->restoreDestroyedOverlap(T), where T is the compressed pointer for P (see GC_SlotObject)
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] forwardedObject A forwarded header containing the original and destination addresses of the object.
	 * @param[in] subSpaceNew Pointer to new space
	 */
	virtual void scavenger_fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject, MM_MemorySubSpaceSemiSpace *subSpaceNew) = 0;
#endif /* OMR_INTERP_COMPRESSED_OBJECT_HEADER */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/**
	 * Enable/disable language specific thread local resource on Concurrent Scavenger cycle start/end 
	 * @param[in] env The environment for the calling thread.
	 */	 
	virtual void scavenger_switchConcurrentForThread(MM_EnvironmentBase *env) = 0;
	/**
	 * After aborted Concurrent Scavenger, handle indirect object references (off heap structures associated with object with refs to Nursery). 
	 * Fixup should update slots to point to the forwarded version of the object and/or remove self forwarded bit in the object itself.
	 */
	virtual void scavenger_fixupIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr) = 0;
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
#endif /* OMR_GC_MODRON_SCAVENGER */

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	/**
	 * Return a pointer to the concurrent GC stats.
	 */
	MM_ConcurrentGCStats *concurrentGC_getConcurrentStats() { return &_concurrentStats; };

	/**
	 * This method is called during card cleaning for each object associated with an uncleaned, dirty card in the card
	 * table. No client actions are necessary but this method may be overridden if desired to hook into card cleaning.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] objectPtr Reference to an object associated with an uncleaned, dirty card.
	 */
	virtual void concurrentGC_processItem(MM_EnvironmentBase *env, omrobjectptr_t objectPtr) {}

	/**
	 * In the case of Weak Consistency platforms we require this method to bring mutator threads to a safe point. A safe
	 * point is a point at which a GC may occur.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @return An instance of a MM_ConcurrentSafepointCallback instance that can signal all mutator threads and cause them
	 * to synchronize at a safe point
	 * @see MM_ConcurrentSafepointCallback
	 */
	virtual MM_ConcurrentSafepointCallback* concurrentGC_createSafepointCallback(MM_EnvironmentBase *env) = 0;

	/**
	 * This method must return a pointer to a signal handling function that will be used by the concurrent helper thread,
	 * along with an optional (NULLable) pointer to an opaque data structure that will be supplied to the signal handling
	 * function whenever it is called.
	 *
	 * @param[out] signalHandlerArg A pointer that will receive a pointer to an opaque data structure that will be supplied
	 * to the signal handling function whenever it is called
	 * @return A pointer to the signal handling function
	 */
	virtual omrsig_handler_fn concurrentGC_getProtectedSignalHandler(void **signalHandlerArg) = 0;

	/**
	 * This method will be called when concurrent initialization is complete. There are no specific requirements for the
	 * implementation of this method but it may serve to process client-language concerns when concurrent initialization
	 * completes.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void concurrentGC_concurrentInitializationComplete(MM_EnvironmentStandard *env) = 0;

	/**
	 * This method may be used to force concurrent kickoff in certain contexts (eg, when determining whether a GC should
	 * be started). A concurrent GC cycle will be started if this method returns true.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] gcCode the GC code to use for the concurrent GC cycle
	 * @param[out] languagekickoffReason a numeric value indicating the client language reason for forcing kickoff
	 * @return true if a concurrent GC kickoff is required
	 */
	virtual bool concurrentGC_forceConcurrentKickoff(MM_EnvironmentBase *env, uintptr_t gcCode, uintptr_t *languagekickoffReason) = 0;

	/* Client language can define an enumeration of constant values to denote language-specific
	 * marking tasks that can be parallelized.
	 *
	 * This enum extends ConcurrentStatus with values >CONCURRENT_ROOT_TRACING and <CONCURRENT_TRACE_ONLY.
	 * Values from this range and from ConcurrentStatus are treated as uintptr_t values everywhere except
	 * when used as case labels in switch() statements where manifest constants are required.
	 *
	 * ConcurrentStatus extensions allow the client language to define discrete units of work
	 * that can be executed in parallel by concurrent threads. ConcurrentGC will call
	 * MM_CollectorLanguageInterface::concurrentGC_collectRoots(..., concurrentStatus, ...)
	 * only once with each client-defined status value. The thread that receives the call
	 * can check the concurrentStatus value to select and execute the appropriate unit of work.
	 *
	 * ConcurrentGC will query the CLI to determine the next concurrent status value by calling
	 * MM_CollectorLanguageInterface::concurrentGC_getNextTracingMode(currentStatus), with an
	 * initial value of CONCURRENT_ROOT_TRACING. The returned value will be passed to
	 * MM_CollectorLanguageInterface::concurrentGC_collectRoots(..., value, ...). When the
	 * client language has processed all status extension values, it must terminate root
	 * tracing by returning CONCURRENT_TRACE_ONLY from  concurrentGC_getNextTracingMode().
	 *
	 * @param[in] executionMode The current execution mode
	 * @return the next execution mode (CONCURRENT_ROOT_TRACING < nextExecutionMode <= CONCURRENT_TRACE_ONLY)
	 */
	virtual uintptr_t concurrentGC_getNextTracingMode(uintptr_t executionMode) = 0;

	/**
	 * After concurrent GC is kicked off, mutator threads are required to contribute by paying an allocation tax
	 * whenever they attempt to allocate new objects. Concurrent root tracing can be partitioned into discrete,
	 * disjoint work units that can be executed concurrently. The client language is responsible for identifying
	 * and enumerating these work units corresponding to the ConcurrentStatus extensions as described for
	 * concurrentGC_getNextTracingMode().
	 *
	 * As each mutator thread pays its allocation tax and the execution mode shifts into the range reserved
	 * for client extensions CONCURRENT_ROOT_TRACING < executionMode < CONCURRENT_TRACE_ONLY), this method
	 * is called to allow the mutator thread to pay some or all of its allocation tax. The enumerator for the mode
	 * to execute is supplied as a input parameter.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] concurrentStatus The current execution mode
	 * @param[out] collectedRoots Set this to true if the work unit was completed
	 * @param[out] Set this to true if all mutator tax paid
	 */
	virtual uintptr_t concurrentGC_collectRoots(MM_EnvironmentStandard *env, uintptr_t concurrentStatus, bool *collectedRoots, bool *paidTax) = 0;

	/**
	 * Tracing a mutator thread stack can only be accomplished safely when the thread is at as safe point. This
	 * method is called when concurrent GC is ready to trace mutator stacks. The implementation must signal each
	 * mutator thread to force it to trace its stack at the next safe point. Each mutator thread receiving this
	 * signal (at a safe point) must call memorySubSpaceAsynchCallbackHandler(OMR_VMThread *) to trace its stack.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void concurrentGC_signalThreadsToTraceStacks(MM_EnvironmentStandard *env) = 0;

	/**
	 * When a concurrent GC cycle is in progress mutators are required to mark cards dirty in the card table
	 * whenever they change the object reference graph. Once concurrent GC initialization is complete this
	 * method will be called. The implementation must ensure that each mutator thread is conditioned to
	 * dirty cards no later then its next safe point and continue to dirty cards until told to stop (see
	 * concurrentGC_signalThreadsToStopDirtyingCards()).
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void concurrentGC_signalThreadsToDirtyCards(MM_EnvironmentStandard *env) = 0;

	/**
	 * This method is called after a concurrent GC cycle has completed. the implementation may inform each
	 * mutator thread to stop dirtying cards. Note that it is acceptable (but less performant) for all
	 * mutator threads to always dirty cards).
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void concurrentGC_signalThreadsToStopDirtyingCards(MM_EnvironmentStandard *env) = 0;

	/**
	 * This method is called after root tracing has completed (executionMode == CONCURRENT_TRACE_ONLY) if
	 * the current estimate of available free space is below the card cleaning threshold. No specific
	 * actions are required but the client language may perform some client-specific work here.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void concurrentGC_kickoffCardCleaning(MM_EnvironmentStandard *env) = 0;

	/**
	 * This method is called when a concurrent GC cycle is aborted. No specific actions are required but
	 * the client language may perform some client-specific work here.
	 *
	 * @param[in] env The environment for the calling thread.
	 *
	 */
	/* TODO: This method name is too Java-specific. Rename to concurrentGC_abortCollection() */
	virtual void concurrentGC_flushRegionReferenceLists(MM_EnvironmentBase *env) = 0;

	/**
	 * This method is called when a concurrent unit of work is completed by a mutator thread while
	 * paying its allocation tax, for example, after every call to concurrentGC_collectRoots() that
	 * returns with taxPaid==true. No specific actions are required but the client language may perform
	 * some client-specific work here.
	 *
	 * @param[in] env The environment for the calling thread.
	 *
	 */
	/* TODO: This method name is too Java-specific. Rename to concurrentGC_flushConcurrentRoots() */
	virtual void concurrentGC_flushThreadReferenceBuffer(MM_EnvironmentBase *env) = 0;

	/**
	 * This method is called to ensure that a mutator thread is not holding any object references that
	 * have not previously been flushed in concurrentGC_flushThreadReferenceBuffer(). This invariant
	 * must hold when a mutator threads begins/ends paying allocation tax, and at the beginning of a
	 * GC cycle.
	 *
	 * @param[in] env The environment for the calling thread.
	 *
	 */
	/* TODO: This method name is too Java-specific. Rename to concurrentGC_hasFlushedConcurrentRoots() */
	virtual bool concurrentGC_isThreadReferenceBufferEmpty(MM_EnvironmentBase *env) = 0;

	/**
	 * This method may be used to kick off a client-specific marking task after root collection is complete
	 * ie, after concurrentGC_getNextTracingMode() returns CONCURRENT_TRACE_ONLY. This task will run concurrently
	 * with other mutator and GC threads that are engaged in concurrent marking.
	 *
	 * Implementation should frequently check for pending exclusive access request and exit in that case.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[out] bytesTraced The number of bytes traced
	 * @param[out] collectedRoots Return true unless exited prematurely to defer to exclusive access request
	 */
	virtual bool concurrentGC_startConcurrentScanning(MM_EnvironmentStandard *env, uintptr_t *bytesTraced, bool *collectedRoots) = 0;

	/**
	 * If concurrentGC_startConcurrentScanning() returns true, this method will be called to inform the client of
	 * the status of concurrent scanning.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[out] isConcurrentScanningComplete true if concurrentGC_startConcurrentScanning() reports >0 bytes traced.
	 */
	virtual void concurrentGC_concurrentScanningStarted(MM_EnvironmentStandard *env, bool isConcurrentScanningComplete) = 0;

	/**
	 * This method will be called to determine whether concurrent scanning has completed.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @return true if concurrent scanning was not started or is complete
	 */
	virtual bool concurrentGC_isConcurrentScanningComplete(MM_EnvironmentBase *env) = 0;

	/**
	 * This method must return an integer-valued code to be reported in the verbose GC log at the end of a concurrent GC cycle.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @return integer-valued code to be reported in the verbose GC log
	 */
	virtual uintptr_t concurrentGC_reportConcurrentScanningMode(MM_EnvironmentBase *env) = 0;


	/**
	 * This method is called to scan the stack of a mutator thread. Implementation must call MM_MarkingScheme::markObject()
	 * for each object reference found on the thread's stack.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	virtual void concurrentGC_scanThread(MM_EnvironmentBase *env) = 0;
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */


	/**
	 * This method is called to check if vm is in Startup stage, return true if vm is in Startup stage(Default: false).
	 *
	 * @param[in] env The environment for the calling thread.
	 * @return true if vm is in Startup stage.
	 */
	virtual bool isVMInStartupPhase(MM_EnvironmentBase *env) { return false; }

	MM_CollectorLanguageInterface()
		: MM_BaseVirtual()
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* COLLECTORLANGUAGEINTERFACE_HPP_ */
