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

#if !defined(ENVIRONMENTBASECORE_HPP_)
#define ENVIRONMENTBASECORE_HPP_

#include "omrcomp.h"
#include "modronbase.h"
#include "omr.h"
#include "thread_api.h"

#include "BaseVirtual.hpp"
#include "CardCleaningStats.hpp"
#include "CycleState.hpp"
#include "CompactStats.hpp"
#include "EnvironmentDelegate.hpp"
#include "GCCode.hpp"
#include "GCExtensionsBase.hpp"
#include "LargeObjectAllocateStats.hpp"
#include "MarkStats.hpp"
#include "RootScannerStats.hpp"
#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)
#include "ScavengerHotFieldStats.hpp"
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */
#include "ScavengerStats.hpp"
#include "SweepStats.hpp"
#include "WorkPacketStats.hpp"
#include "WorkStack.hpp"

class MM_AllocationContext;
class MM_AllocateDescription;
class MM_Collector;
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
class MM_ConcurrentGCStats;
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
class MM_HeapRegionQueue;
class MM_MemorySpace;
class MM_ObjectAllocationInterface;
class MM_SegregatedAllocationTracker;
class MM_Task;
class MM_Validator;

/* Allocation color values -- also used in bit in Metronome -- see Metronome.hpp */
#define GC_UNMARK	0
#define GC_MARK		0x20

/**
 * Type of thread.
 * @ingroup GC_Base_Core
 */
typedef enum {
	MUTATOR_THREAD = 1,
	WRITE_BARRIER_THREAD,
	CON_MARK_HELPER_THREAD,
	GC_SLAVE_THREAD,
	GC_MASTER_THREAD
} ThreadType;

/**
 * Provide information on the base environment
 * @ingroup GC_Base_Core
 */
class MM_EnvironmentBase : public MM_BaseVirtual
{
private:
	uintptr_t _slaveID;
	uintptr_t _environmentId;

protected:
	OMR_VM *_omrVM;
	OMR_VMThread *_omrVMThread;
	OMRPortLibrary *_portLibrary; /**< the port library associated with the environment */
	MM_EnvironmentDelegate _delegate;

private:
	uintptr_t _workUnitIndex;
	uintptr_t _workUnitToHandle;

	bool _threadScanned;

	MM_AllocationContext *_allocationContext;	/**< The "second-level caching mechanism" for this thread */
	MM_AllocationContext *_commonAllocationContext;	/**< Common Allocation Context shared by all threads */


	uint64_t _exclusiveAccessTime; /**< time (in ticks) of the last exclusive access request */
	uint64_t _meanExclusiveAccessIdleTime; /**< mean idle time (in ticks) of the last exclusive access request */
	OMR_VMThread* _lastExclusiveAccessResponder; /**< last thread to respond to last exclusive access request */
	uintptr_t _exclusiveAccessHaltedThreads; /**< number of threads halted by last exclusive access request */
	bool _exclusiveAccessBeatenByOtherThread; /**< true if last exclusive access request had to wait for another GC thread */
	uintptr_t _exclusiveCount; /**< count of number of times this thread has acquired but not yet released exclusive access */

protected:
	bool _allocationFailureReported;	/**< verbose: used to report af-start/af-end once per allocation failure even more then one GC cycle need to resolve AF */

#if defined(OMR_GC_SEGREGATED_HEAP)
	MM_HeapRegionQueue* _regionWorkList;
	MM_HeapRegionQueue* _regionLocalFree;
	MM_HeapRegionQueue* _regionLocalFull; /* cached full region queue per slave during sweep */
#endif /* OMR_GC_SEGREGATED_HEAP */

public:
	MM_ObjectAllocationInterface *_objectAllocationInterface; /**< Per-thread interface that guides object allocation decisions */

	MM_WorkStack _workStack;

	ThreadType  _threadType;
	MM_CycleState *_cycleState;	/**< The current GC cycle that this thread is operating on */
	bool _isInNoGCAllocationCall;	/**< NOTE:  this is a "best-efforts" flag, only, for use by an assertion in the collector.  If this is true, the owning thread is attempting to perform a NoGC allocation.  If it is false, nothing can be implied about the owning thread's state. */
	bool _failAllocOnExcessiveGC;

	MM_Task *_currentTask;
	
	MM_WorkPacketStats _workPacketStats;
	MM_WorkPacketStats _workPacketStatsRSScan;   /**< work packet Stats specifically for RS Scan Phase of Concurrent STW GC */

	uint64_t _slaveThreadCpuTimeNanos;	/**< Total CPU time used by this slave thread (or 0 for non-slaves) */

	MM_FreeEntrySizeClassStats _freeEntrySizeClassStats;  /**< GC thread local statistics structure for heap free entry size (sizeClass) distribution */

	uintptr_t _oolTraceAllocationBytes; /**< Tracks the bytes allocated since the last ool object trace */

	MM_Validator *_activeValidator; /**< Used to identify and report crashes inside Validators */

	MM_MarkStats _markStats;

	MM_RootScannerStats _rootScannerStats; /**< Per thread stats to track the performance of the root scanner */

	const char * _lastSyncPointReached; /**< string indicating latest sync point reached by this associated env's thread */

#if defined(OMR_GC_SEGREGATED_HEAP)
	MM_SegregatedAllocationTracker* _allocationTracker; /**< tracks bytes allocated per thread and periodically flushes allocation data to MM_MemoryPoolSegregated */
#endif /* OMR_GC_SEGREGATED_HEAP */

	volatile uint32_t _allocationColor; /**< Flag field to indicate whether premarking is enabled on the thread */

#if defined(OMR_GC_HEAP_CARD_TABLE)
	MM_CardCleaningStats _cardCleaningStats; /**< Per thread stats to track the performance of the card cleaning */
#endif /* defined(OMR_GC_HEAP_CARD_TABLE) */
#if defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME)
	MM_SweepStats _sweepStats;
#if defined(OMR_GC_MODRON_COMPACTION)
	MM_CompactStats _compactStats;
#endif /* OMR_GC_MODRON_COMPACTION */
#endif /* OMR_GC_MODRON_STANDARD || OMR_GC_REALTIME */
#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)
	MM_ScavengerStats _scavengerStats;
	MM_ScavengerHotFieldStats _hotFieldStats; /**< hot field statistics for this GC thread */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	uint64_t _concurrentScavengerSwitchCount; /**< local counter of cycle start and cycle end transitions */
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */

private:

protected:
	virtual bool initialize(MM_GCExtensionsBase *extensions);
	virtual void tearDown(MM_GCExtensionsBase *extensions);
	MMINLINE void setEnvironmentId(uintptr_t environmentId) {_environmentId = environmentId;}

	void reportExclusiveAccessRelease();
	void reportExclusiveAccessAcquire();

public:
	static MM_EnvironmentBase *newInstance(MM_GCExtensionsBase *extensions, OMR_VMThread *vmThread);

	/**
	 * Get the Core Environment.
	 * @return Pointer to the core environment.
	 */
	MMINLINE static MM_EnvironmentBase *getEnvironment(OMR_VMThread *omrVMThread) { return (MM_EnvironmentBase *)omrVMThread->_gcOmrVMThreadExtensions; }
	virtual void kill();

	/**
	 * Get the base extensions
	 * @return The base extensions
	 */
	MMINLINE MM_GCExtensionsBase *getExtensions() { return (MM_GCExtensionsBase *)_omrVM->_gcOmrVMExtensions; }

	MMINLINE MM_MemorySpace *getMemorySpace() { return (MM_MemorySpace*)(_omrVMThread->memorySpace); }

	MM_MemorySubSpace *getDefaultMemorySubSpace();
	MM_MemorySubSpace *getTenureMemorySubSpace();

	/**
	 * Get a pointer to the J9VMThread structure.
	 * @return Pointer to the J9VMThread structure.
	 */
	MMINLINE OMR_VMThread *getOmrVMThread() { return _omrVMThread; }
	
	/**
	 * Get a pointer to the OMR_VM structure.
	 * @return Pointer to the OMR_VM structure
	 */
	MMINLINE OMR_VM *getOmrVM() { return _omrVM; }

	MMINLINE void *
	getLanguageVMThread()
	{
		if (NULL != _omrVMThread) {
			return _omrVMThread->_language_vmthread;
		}
		return NULL;
	}

	MMINLINE void *getLanguageVM() {return _omrVM->_language_vm;}

	/**
	 * Get run-time object alignment in bytes
	 * @return object alignment in bytes
	 */
	MMINLINE uintptr_t
	getObjectAlignmentInBytes()
	{
		return getExtensions()->getObjectAlignmentInBytes();
	}

	/**
	 * Get a pointer to the port library.
	 * @return Pointer to the port library.
	 */
	MMINLINE OMRPortLibrary *getPortLibrary() { return _portLibrary; }
	
	/**
	 * Get the memory forge
	 * @return The memory forge
	 */
	MMINLINE MM_Forge *getForge() { return getExtensions()->getForge(); }

	/**
	 * Get the thread's priority.
	 * @return The thread's priority.
	 */
	MMINLINE uintptr_t getPriority() { return omrthread_get_priority(_omrVMThread->_os_thread); }
	
	/**
	 * Set the the thread's priority.
	 * @param priority The priority to set the thread to.
	 * @returns 0 on success or negative value on failure (priority wasn't changed)
	 */
	MMINLINE intptr_t setPriority(uintptr_t priority) { return omrthread_set_priority(_omrVMThread->_os_thread, priority); }

	/**
	 * @deprecated  This function needs to be replaced with one which can describe a set of nodes
	 * Determine what the NUMA node affinity is for the thread.
	 *
	 * @return the index of the node to associate with, where 1 is the first node. (0 indicates no affinity)
	 */
	MMINLINE uintptr_t 
	getNumaAffinity() 
	{ 
		uintptr_t result = 0;
		uintptr_t nodeCount = 1;
		if ((0 != omrthread_numa_get_node_affinity(_omrVMThread->_os_thread, &result, &nodeCount)) || (0 == nodeCount)) {
			result = 0;
		}
		return result;
	};
	
	/**
	 * @deprecated  This function needs to be replaced with one which can describe a set of nodes
	 * Set the affinity for the thread so that it runs only (or preferentially) on
	 * processors associated with the specified NUMA node.
	 * 
	 * @param numaNodes[in] The array of node numbers to associate with, where 1 is the first node. (0 indicates no affinity)
	 * @param arrayLength[in] The number of elements in numaNodes (must be at least 1)
	 *
	 * @return true on success, false on failure 
	 */
	MMINLINE bool setNumaAffinity(uintptr_t *numaNodes, uintptr_t arrayLength) { return 0 == omrthread_numa_set_node_affinity(_omrVMThread->_os_thread, numaNodes, arrayLength, 0); }
		
	/**
	 * Get the threads slave id.
	 * @return The threads slave id.
	 */
	MMINLINE uintptr_t getSlaveID() { return _slaveID; }

	/**
	 * Sets the threads slave id.
	 */
	MMINLINE void setSlaveID(uintptr_t slaveID) { _slaveID = slaveID; }

	/**
	 * Enguires if this thread is the master.
	 * return true if the thread is the master thread, false otherwise.
	 */
	 MMINLINE bool isMasterThread() { return _slaveID == 0; }

	/**
	 * Gets the threads type.
	 * @return The type of thread.
	 */
	MMINLINE ThreadType getThreadType() { return _threadType; } ;

	/**
	 * Sets the threads type.
	 * @param threadType The thread type to set thread to.
	 */
	MMINLINE void setThreadType(ThreadType threadType) {_threadType= threadType; } ;

	/**
	 * Gets the id used for calculating work packet sublist indexes.
	 * @return The environment's packet sublist id.
	 */
	MMINLINE uintptr_t getEnvironmentId() { return _environmentId; }

	/**
	 * Set the vmState to that supplied, and return the previous
	 * state so it can be restored later
	 */
	uintptr_t pushVMstate(uintptr_t newState);

	/**
	 * Restore the vmState to that supplied (which should have
	 * been previously obtained through a call to @ref pushVMstate()
	 */
	void popVMstate(uintptr_t newState);

	/**
	 * Temporarily save a reference to an object that has been allocated but
	 * not yet linked into the object reference tree. The object will be
	 * treated as a root reference if a GC occurs while the object is
	 * saved here.
	 *
	 * Must be balanced with an equal number of calls to restore objects.
	 */
	bool saveObjects(omrobjectptr_t objectPtr);

	/**
	 * Restore a reference to an object that has been allocated but
	 * not yet linked into the object reference tree. It is presumed that
	 * the object will be linked into the mutator's reference tree after
	 * it is received from this call.
	 *
	 * Object must previously have been saved via saveObjects().
	 */
	void restoreObjects(omrobjectptr_t *objectPtrIndirect);

	/**
	 *	Verbose: allocation Failure Start Report if required
	 *	set flag allocation Failure Start Report required
	 *	@param allocDescription Allocation Description
	 *	@param flags current memory subspace flags
	 */
	void allocationFailureStartReportIfRequired(MM_AllocateDescription *allocDescription, uintptr_t flags);

	/**
	 *	Verbose: allocation Failure End Report if required
	 *	clear flag allocation Failure Start Report required
	 *	@param allocDescription - The current threads allocation description
	 */
	void allocationFailureEndReportIfRequired(MM_AllocateDescription *allocDescription);

	/**
	 * Acquires shared VM access.
	 */
	void acquireVMAccess();

	/**
	 * Releases shared VM access.
	 */
	void releaseVMAccess();

	/**
	 * Try and acquire exclusive access if no other thread is already requesting it.
	 * Make an attempt at acquiring exclusive access if the current thread does not already have it.  The
	 * attempt will abort if another thread is already going for exclusive, which means this
	 * call can return without exclusive access being held.  As well, this call will block for any other
	 * requesting thread, and so should be treated as a safe point.
	 * @note call can release VM access.
	 * @return true if exclusive access was acquired, false otherwise.
	 */
	bool tryAcquireExclusiveVMAccess();

	/**
	 * Checks to see if the thread has exclusive access
	 * @return true if the thread has exclusive access, false if not.
	 */
	bool
	inquireExclusiveVMAccessForGC()
	{
		return (_exclusiveCount > 0);
	}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	/**
	 * Try and acquire exclusive access if no other thread is already requesting it.
	 * Make an attempt at acquiring exclusive access if the current thread does not already have it.  The
	 * attempt will abort if another thread is already going for exclusive, which means this
	 * call can return without exclusive access being held, it will also abort if another another thread
	 * may have beat us to it and prepared the threads or even collected. As well, this call will block for
	 * any other requesting thread, and so should be treated as a safe point.
	 * @note call can release VM access.
	 * @return true if exclusive access was acquired, false otherwise.
	 */
	bool tryAcquireExclusiveForConcurrentKickoff(MM_ConcurrentGCStats *stats);

	/**
	 * Release exclusive access.
	 * The calling thread will release one level (recursion) of its exclusive access request, and alert other threads that it has completed its
	 * intended GC request if this is the last level.
	 */
	void releaseExclusiveForConcurrentKickoff();
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

	/**
	 * Attempt to acquire exclusive access to request a gc.
	 * If the thread is beaten to acquiring exclusive access by another thread then the caller will not acquire exclusive access.
	 * @param collector gc intended to be used for collection.
	 * @return boolean indicating whether the thread now holds exclusive access (ie: it won the race).
	 *
	 * @note this call should be considered a safe-point as the thread may release VM access to allow the winning thread to acquire exclusivity.
	 * @note this call supports recursion.
	 */
	bool tryAcquireExclusiveVMAccessForGC(MM_Collector *collector);

	/**
	 * Acquire exclusive access to request a gc.
	 * The calling thread will acquire exclusive access for Gc regardless if other threads beat it to exclusive for the same purposes.
	 * @param collector gc intended to be used for collection.
	 * @return boolean indicating whether the thread cleanly won exclusive access for its collector or if it had been beaten already.
	 *
	 * @note this call should be considered a safe-point as the thread may release VM access to allow the other threads to acquire exclusivity.
	 * @note this call supports recursion.
	 */
	bool acquireExclusiveVMAccessForGC(MM_Collector *collector);

	/**
	 * Release exclusive access.
	 * The calling thread will release one level (recursion) of its exclusive access request, and alert other threads that it has completed its
	 * intended GC request if this is the last level.
	 */
	void releaseExclusiveVMAccessForGC();

	/**
	 * Release all recursive levels of exclusive access.
	 * The calling thread will release all levels (recursion) of its exclusive access request, and alert other threads that it has completed its
	 * intended GC request.
	 */
	void unwindExclusiveVMAccessForGC();

	/**
	 * Acquire exclusive VM access.
	 */
	void acquireExclusiveVMAccess();

	/**
	 * Releases exclusive VM access.
	 */
	void releaseExclusiveVMAccess();
	
	/**
	 * Give up exclusive access in preparation for transferring it to a collaborating thread (i.e. main-to-master or master-to-main)
	 * @return the exclusive count of the current thread before relinquishing 
	 */
	uintptr_t relinquishExclusiveVMAccess();

	/**
	 * Assume exclusive access from a collaborating thread i.e. main-to-master or master-to-main)
	 * @param exclusiveCount the exclusive count to be restored 
	 */
	void assumeExclusiveVMAccess(uintptr_t exclusiveCount);

	/**
	 * Checks to see if any thread has requested exclusive access
	 * @return true if a thread is waiting on exclusive access, false if not.
	 */
	bool isExclusiveAccessRequestWaiting();

	/**
	 * Get the time taken to acquire exclusive access.
	 * Time is stored in raw format (no units).  Output routines
	 * are responsible for converting to the desired resolution (msec, usec)
	 * @return The time taken to acquire exclusive access.
	 */
	uint64_t getExclusiveAccessTime() { return _exclusiveAccessTime; };

	/**
	 * Get the time average threads were idle while acquiring exclusive access.
	 * Time is stored in raw format (no units).  Output routines
	 * are responsible for converting to the desired resolution (msec, usec)
	 * @return The mean idle time during exclusive access acquisition.
	 */
	uint64_t getMeanExclusiveAccessIdleTime() { return _meanExclusiveAccessIdleTime; };

	/**
	 * Get the last thread to respond to the exclusive access request.
	 * @return The last thread to respond
	 */
	OMR_VMThread* getLastExclusiveAccessResponder() { return _lastExclusiveAccessResponder; };

	/**
	 * Get the number of threads which were halted for the exclusive access request.
	 * @return The number of halted threads
	 */
	uintptr_t getExclusiveAccessHaltedThreads() { return _exclusiveAccessHaltedThreads; };

	/**
	 * Enquire whether we were beaten to exclusive access by another thread.
	 * @return true if we were beaten, false otherwise.
	 */
	bool exclusiveAccessBeatenByOtherThread() { return _exclusiveAccessBeatenByOtherThread; }

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/**
	 * Force thread to use out-of-line request for VM access. This may be required if there
	 * is there is an event waiting to be hooked the next time the thread acquires VM access.
	 */
	void forceOutOfLineVMAccess() { _delegate.forceOutOfLineVMAccess(); }
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
	/**
	 * Disable inline TLH allocates by hiding the real heap allocation address from
	 * JIT/Interpreter in realHeapAlloc and setting heapALloc == HeapTop so TLH
	 * looks full.
	 *
	 */
	void disableInlineTLHAllocate() { _delegate.disableInlineTLHAllocate(); }

	/**
	 * Re-enable inline TLH allocate by restoring heapAlloc from realHeapAlloc
	 */
	void enableInlineTLHAllocate() { _delegate.enableInlineTLHAllocate(); }

	/**
	 * Determine if inline TLH allocate is enabled; its enabled if realheapAlloc is NULL.
	 * @return TRUE if inline TLH allocates currently enabled for this thread; FALSE otherwise
	 */
	bool isInlineTLHAllocateEnabled() { return _delegate.isInlineTLHAllocateEnabled(); }
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	MMINLINE uintptr_t getWorkUnitIndex() { return _workUnitIndex; }
	MMINLINE uintptr_t getWorkUnitToHandle() { return _workUnitToHandle; }
	MMINLINE void setWorkUnitToHandle(uintptr_t workUnitToHandle) { _workUnitToHandle = workUnitToHandle; }
	MMINLINE uintptr_t nextWorkUnitIndex() { return _workUnitIndex++; }
	MMINLINE void resetWorkUnitIndex() {
		_workUnitIndex = 1;
		_workUnitToHandle = 0;
	}

	MMINLINE void setThreadScanned(bool threadScanned) { _threadScanned = threadScanned; };
	MMINLINE bool isThreadScanned() { return _threadScanned; };

	/**
	 * Initialization specifically for GC threads
	 */
	virtual void initializeGCThread() {}

	MM_GCCode getCycleStateGCCode() { return _cycleState->_gcCode; }

	/**
	 * Return back pointer to thread's Allocation Context
	 */
	MMINLINE MM_AllocationContext *getAllocationContext() const { return _allocationContext; }

	/**
	 * Set pointer to thread's Allocation Context
	 * param[in] allocationContext pointer to context to set
	 */
	MMINLINE void setAllocationContext(MM_AllocationContext *allocationContext) { _allocationContext = allocationContext; }

	/**
	 * Return back pointer to Common Allocation Context
	 */
	MMINLINE MM_AllocationContext *getCommonAllocationContext() const { return _commonAllocationContext; }

	/**
	 * Set pointer to Common Allocation Context
	 * param[in] commonAllocationContext pointer to context to set
	 */
	MMINLINE void setCommonAllocationContext(MM_AllocationContext *commonAllocationContext) { _commonAllocationContext = commonAllocationContext; }

	MMINLINE uint32_t getAllocationColor() const { return _allocationColor; }
	MMINLINE void setAllocationColor(uint32_t allocationColor) { _allocationColor = allocationColor; }

	MMINLINE MM_WorkStack *getWorkStack() { return &_workStack; }

	/**
	 * Get a pointer to common GC metadata attached to this environment. The GC environment structure
	 * is defined by the client language and bound to the environment delegate attached to this class.
	 * It is typically used to hold thread-local GC-related stats and caches that are merged at key
	 * points into global structures.
	 */
	MMINLINE GC_Environment *getGCEnvironment() { return _delegate.getGCEnvironment(); }

	/**
	 * Called on each participating GC thread before starting mark phase. This can be used
	 * to reset local thread stats and reference buffers prior to commencing marking.
	 */
	MMINLINE void
	markingPhaseStarted()
	{
		_markStats.clear();
		_workPacketStats.clear();

		_delegate.markingStarted();

#if defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME)
		/* record that this thread is participating in this cycle */
		_markStats._gcCount = _workPacketStats._gcCount = getExtensions()->globalGCStats.gcCount;
#endif /* defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME) */
	}

	/**
	 * Called on each participating GC thread after completing mark phase. This can be used
	 * to merge local thread stats and reference buffers into global containers to compleete marking.
	 */
	MMINLINE void
	markingPhaseFinished()
	{
		_delegate.markingFinished();

#if defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME)
		MM_GCExtensionsBase *extensions = getExtensions();
		extensions->globalGCStats.markStats.merge(&_markStats);
		extensions->globalGCStats.workPacketStats.merge(&_workPacketStats);
#endif /* defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME) */
	}

#if defined(OMR_GC_SEGREGATED_HEAP)
	MMINLINE MM_HeapRegionQueue *getRegionWorkList() const { return _regionWorkList; }
	MMINLINE MM_HeapRegionQueue *getRegionLocalFree() const { return _regionLocalFree; }
	MMINLINE MM_HeapRegionQueue *getRegionLocalFull() const { return _regionLocalFull; }
#endif /* OMR_GC_SEGREGATED_HEAP */

	/**
	 * Create an EnvironmentBase object.
	 */
	MM_EnvironmentBase(OMR_VMThread *omrVMThread) :
		MM_BaseVirtual()
		,_slaveID(0)
		,_environmentId(0)
		,_omrVM(omrVMThread->_vm)
		,_omrVMThread(omrVMThread)
		,_portLibrary(omrVMThread->_vm->_runtime->_portLibrary)
		,_delegate()
		,_workUnitIndex(0)
		,_workUnitToHandle(0)
		,_threadScanned(false)
		,_allocationContext(NULL)
		,_commonAllocationContext(NULL)
		,_exclusiveAccessTime(0)
		,_meanExclusiveAccessIdleTime(0)
		,_lastExclusiveAccessResponder(NULL)
		,_exclusiveAccessHaltedThreads(0)
		,_exclusiveAccessBeatenByOtherThread(false)
		,_exclusiveCount(0)
		,_allocationFailureReported(false)
#if defined(OMR_GC_SEGREGATED_HEAP)
		,_regionWorkList(NULL)
		,_regionLocalFree(NULL)
		,_regionLocalFull(NULL)
#endif /* OMR_GC_SEGREGATED_HEAP */
		,_objectAllocationInterface(NULL)
		,_workStack()
		,_threadType(MUTATOR_THREAD)
		,_cycleState(NULL)
		,_isInNoGCAllocationCall(false)
		,_failAllocOnExcessiveGC(false)
		,_currentTask(NULL)
		,_slaveThreadCpuTimeNanos(0)
		,_freeEntrySizeClassStats()
		,_oolTraceAllocationBytes(0)
		,_activeValidator(NULL)
		,_lastSyncPointReached(NULL)
#if defined(OMR_GC_SEGREGATED_HEAP)
		,_allocationTracker(NULL)
#endif /* OMR_GC_SEGREGATED_HEAP */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		,_concurrentScavengerSwitchCount(0)
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */

	{
		_typeId = __FUNCTION__;
	}

	MM_EnvironmentBase(OMR_VM *omrVM) :
		MM_BaseVirtual()
		,_slaveID(0)
		,_environmentId(0)
		,_omrVM(omrVM)
		,_omrVMThread(NULL)
		,_portLibrary(omrVM->_runtime->_portLibrary)
		,_workUnitIndex(0)
		,_workUnitToHandle(0)
		,_threadScanned(false)
		,_allocationContext(NULL)
		,_commonAllocationContext(NULL)
		,_exclusiveAccessTime(0)
		,_meanExclusiveAccessIdleTime(0)
		,_lastExclusiveAccessResponder(NULL)
		,_exclusiveAccessHaltedThreads(0)
		,_exclusiveAccessBeatenByOtherThread(false)
		,_exclusiveCount(0)
		,_allocationFailureReported(false)
#if defined(OMR_GC_SEGREGATED_HEAP)
		,_regionWorkList(NULL)
		,_regionLocalFree(NULL)
		,_regionLocalFull(NULL)
#endif /* OMR_GC_SEGREGATED_HEAP */
		,_objectAllocationInterface(NULL)
		,_workStack()
		,_threadType(MUTATOR_THREAD)
		,_cycleState(NULL)
		,_isInNoGCAllocationCall(false)
		,_failAllocOnExcessiveGC(false)
		,_currentTask(NULL)
		,_slaveThreadCpuTimeNanos(0)
		,_freeEntrySizeClassStats()
		,_oolTraceAllocationBytes(0)
		,_activeValidator(NULL)
		,_lastSyncPointReached(NULL)
#if defined(OMR_GC_SEGREGATED_HEAP)
		,_allocationTracker(NULL)
#endif /* OMR_GC_SEGREGATED_HEAP */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		,_concurrentScavengerSwitchCount(0)
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* ENVIRONMENTBASECORE_HPP_ */
