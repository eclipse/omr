/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * TODO:
 */
#ifndef EVACUATORCONTROLLER_HPP_
#define EVACUATORCONTROLLER_HPP_

#include "omr.h"
#include "omrcfg.h"

#include "Collector.hpp"
#include "CollectorLanguageInterface.hpp"
#include "EnvironmentStandard.hpp"
#include "Evacuator.hpp"
#include "EvacuatorBase.hpp"
#include "EvacuatorHistory.hpp"
#include "EvacuatorParallelTask.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "ParallelDispatcher.hpp"

class GC_ObjectScanner;
class MM_EvacuatorWhitespace;
class MM_MemorySubSpace;
class MM_MemorySubSpaceSemiSpace;

/**
 * Whitespace (MM_EvacuatorWhitespace) is free space for copying and may be free or bound to a
 * whitelist (MM_EvacuatorWhitelist) or to a scan or copy space (MM_EvacuatorScanspace, MM_EvacuatorCopyspace).
 * The whitelist is a priority queue, presenting largest available whitespace on top. Allocation
 * requests for whitespace are always satisfied by the whitelist if the top whitespace can accommodate,
 * new blocks of whitespace are allocated from survivor and tenure memory subspaces as a last resort.
 *
 * Work is unscanned copy. Each evacuator (MM_Evacuator) maintains a stack of scanspaces
 * for hierarchical (inside) copying and two copyspaces (survivor and tenure) to copy breadth-first
 * objects that are not copied inside the current scanspace. Work is released from outside copyspaces
 * when its size exceeds a threshold imposed by the controller (an instance of this class).
 *
 * The controller periodically sums copied/scanned byte counts over all evacuators to obtain epochal
 * information about progress of evacuation. Epochal copied/scanned page counts are used to determine
 * evacuator operating parameters for each sampling epoch, including whitespace allocation size for
 * refreshing inside/outside scan/copyspaces and unscanned copy size threshold for releasing work
 * from copyspaces to evacuator work queues.
 *
 * All sizes and lengths are expressed in bytes in controller, evacuator, white/copy/scan
 * space contexts.
 */
class MM_EvacuatorController : public MM_Collector
{
/**
 * Data members
 */
private:
	/* constants for mapping evacuator index to bitmap word and bit offset */
	static const uintptr_t index_to_map_word_shift = 6;
	static const uintptr_t index_to_map_word_modulus = ((uintptr_t)1 << index_to_map_word_shift) - 1;

	static const uintptr_t epochs_per_cycle = MM_EvacuatorHistory::epochs_per_cycle;
	static const uintptr_t allocation_page_size = MM_EvacuatorBase::inside_copy_size;

	typedef MM_Evacuator *EvacuatorPointer;

	const uintptr_t _maxGCThreads;						/* fixed for life of vm */
	EvacuatorPointer * const _evacuatorTask;			/* array of pointers to instantiated evacuators */
	uintptr_t _evacuatorCount;							/* number of gc threads that will participate in collection */
	omrthread_monitor_t	_controllerMutex;				/* synchronize evacuator work distribution and end of scan cycle */
	omrthread_monitor_t	_reporterMutex;					/* synchronize collection of epochal records within gc cycle */
	volatile uint64_t * const _boundEvacuatorBitmap;	/* maps evacuator threads that have been dispatched and bound to an evacuator instance */
	volatile uint64_t * const _stalledEvacuatorBitmap;	/* maps evacuator threads that are stalled (waiting for work) */
	volatile uint64_t * const _resumingEvacuatorBitmap;	/* maps evacuator threads that are resuming or completing scan cycle after stalling */
	volatile uint64_t * const _evacuatorMask;			/* maps all evacuator threads, bound or unbound */
	volatile uintptr_t _stalledEvacuatorCount;			/* number of stalled evacuator threads */
	volatile uintptr_t _evacuatorIndex;					/* number of GC threads that have joined the evacuation so far */
	volatile uintptr_t _evacuatorFlags;					/* private and public (language defined) evacuation flags shared among evauators */
	volatile uintptr_t _copyspaceAllocationCeiling[2];	/* upper bounds for copyspace allocation in survivor, tenure regions */
	volatile uintptr_t _objectAllocationCeiling[2];		/* upper bounds for object allocation in survivor, tenure regions */
	volatile uint64_t _nextEpochCopiedBytesThreshold;	/* threshold for ending current reporting epoch */
	uintptr_t _copiedBytesReportingDelta;				/* delta copied byte count per evacuator for triggering evacuator progress report to controller */
	uint64_t _epochTimestamp;							/* start of current epoch */

	uint8_t *_heapLayout[3][2];							/* generational heap region bounds */
	MM_MemorySubSpace *_memorySubspace[3];				/* pointers to memory subspaces for heap regions */

protected:
#if defined(EVACUATOR_DEBUG) || defined(EVACUATOR_DEBUG_ALWAYS)
	uint64_t _collectorStartTime;						/* collector startup time */
#endif /* defined(EVACUATOR_DEBUG) || defined(EVACUATOR_DEBUG_ALWAYS) */
	const uintptr_t _objectAlignmentInBytes;			/* cached object alignment from GC_ObjectModelBase */
	volatile uint64_t _copiedBytes[2];					/* running total aggregate volume of copy accumulated in current gc cycle */
	volatile uint64_t _scannedBytes;					/* running total aggregate volume of copy scanned in current gc cycle */
	volatile uint64_t _finalDiscardedBytes;				/* sum of final whitespace bytes discarded during gc cycle for all evacuators */
	volatile uint64_t _finalFlushedBytes;				/* sum of final whitespace bytes flushed at end of gc cycle for all evacuators */
	uint64_t _finalEvacuatedBytes;						/* total number of bytes evacuated to survivor/tenure regions during gc cycle */
	uint64_t _globalTenureFlushedBytes;					/* sum of tenure whitespace bytes flushed before global collections and shutdown */

	/* fields pulled down from MM_Scavenger ... */

	MM_GCExtensionsBase * const _extensions;			/* points to GC extensions */
	MM_ParallelDispatcher * const _dispatcher;			/* dispatches evacuator tasks */
	uintptr_t _tenureMask;								/* tenure mask for selecting whether evacuated object should be tenured */
	MM_MemorySubSpaceSemiSpace *_activeSubSpace; 		/* top level new subspace subject to GC */
	MM_MemorySubSpace *_evacuateMemorySubSpace;			/* cached pointer to evacuate subspace within active subspace */
	MM_MemorySubSpace *_survivorMemorySubSpace; 		/* cached pointer to survivor subspace within active subspace */
	MM_MemorySubSpace *_tenureMemorySubSpace;			/* cached pointer to tenure subspace */
	void *_evacuateSpaceBase, *_evacuateSpaceTop;		/* cached base and top heap pointers within evacuate subspace */
	void *_survivorSpaceBase, *_survivorSpaceTop;		/* cached base and top heap pointers within survivor subspace */

	/* ... and history is exposed to MM_Scavenger for end cycle tracing */

	MM_EvacuatorHistory _history;						/* epochal record per gc cycle */

public:
	static const uintptr_t minimumCopyspaceSize = MM_EvacuatorBase::min_tlh_allocation_size; /* hard lower bound for tlh copyspace allocation size */
	static const uintptr_t maximumCopyspaceSize = MM_EvacuatorBase::max_tlh_allocation_size; /* hard upper bound for tlh copyspace allocation size */

	static const uintptr_t minimumWorkspaceSize = MM_EvacuatorBase::min_work_packet_size; /* hard lower bound for work packet size */
	static const uintptr_t maximumWorkspaceSize = MM_EvacuatorBase::min_work_packet_size; /* hard upper bound for work packet size */

	static const uintptr_t max_evacuator_public_flag = (uintptr_t)1 << 15;
	static const uintptr_t min_evacuator_private_flag = (uintptr_t)1 << 16;

	enum {
		rescanThreadSlots 	= min_evacuator_private_flag << 0
		, completing		= min_evacuator_private_flag << 1
		, aborting			= min_evacuator_private_flag << 2
	};

	OMR_VM *_omrVM;
#if defined(EVACUATOR_DEBUG)
	MM_EvacuatorBase _debugger;
#endif /* defined(EVACUATOR_DEBUG) */

/**
 * Function members
 */
private:
	/* lower and upper address bounds for each involved heap region */
	void copyHeapLayout(uint8_t *a[][2], uint8_t *b[][2]);

	/* allocate at least minimumLength, at most optimalLength, bytes of whitespace in specified region */
	MM_EvacuatorWhitespace *allocateWhitespace(MM_Evacuator *evacuator, MM_Evacuator::EvacuationRegion region, uintptr_t remainder, uintptr_t minimumLength, bool inside);

	/* calculate a rough overestimate of the amount of matter that will be evacuated to survivor or tenure in current cycle */
	uint64_t  calculateProjectedEvacuationBytes();

	/* calculate whitespace allocation size considering volume of remaining survivor space and current ratio of unscanned/scanned bytes */
	uintptr_t calculateOptimalWhitespaceSize();

	/* this is the ratio of running evacuators vs evacuator count */
	double calcluateEvacuatorBandwidth();

	/* allocate and NULL-fill evacuator pointer array (evacuators instantiated at gc start as required) */
	static EvacuatorPointer *
	allocateEvacuatorArray(MM_EnvironmentBase *env, uintptr_t maxGCThreads)
	{
		EvacuatorPointer *evacuatorArray = NULL;

		if (env->getExtensions()->isEvacuatorEnabled()) {
			Debug_MM_true(0 < maxGCThreads);
			evacuatorArray = (EvacuatorPointer *)env->getForge()->allocate(sizeof(EvacuatorPointer) * maxGCThreads, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
			Debug_MM_true(NULL != evacuatorArray);
			for (uintptr_t evacuator = 0; evacuator < maxGCThreads; evacuator += 1) {
				evacuatorArray[evacuator] = NULL;
			}
		}

		return evacuatorArray;
	}

	/* allocate and 0-fill evacuator thread bitmap */
	static volatile uint64_t *
	allocateEvacuatorBitmap(MM_EnvironmentBase *env, uintptr_t maxGCThreads)
	{
		volatile uint64_t *map = NULL;

		if (env->getExtensions()->isEvacuatorEnabled()) {
			Debug_MM_true(0 < maxGCThreads);
			uintptr_t mapWords = (maxGCThreads >> index_to_map_word_shift) + ((0 != (maxGCThreads & index_to_map_word_modulus)) ? 1 : 0);
			map = (volatile uint64_t *)env->getForge()->allocate(sizeof(uint64_t) * mapWords, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
			Debug_MM_true(NULL != map);
			for (uintptr_t word = 0; word < mapWords; word += 1) {
				map[word] = 0;
			}
		}

		return map;
	}

	/* evacuator bitmask is traversed in increasing index order, index wraps around to 0 after last bound evacuator */
	uintptr_t
	nextWorkerIndex(uintptr_t workerIndex)
	{
		workerIndex += 1;
		if (workerIndex >= _evacuatorIndex) {
			workerIndex = 0;
		}
		return workerIndex;
	}

	/* get bit mask for evacuator bit as aligned in evacuator bitmap */
	uint64_t
	getEvacuatorBitMask(uintptr_t evacuatorIndex)
	{
		Debug_MM_true(evacuatorIndex < _evacuatorCount);
		return (uint64_t)1 << (evacuatorIndex & index_to_map_word_modulus);
	}

	/* calculate word/bit coordinates for worker index */
	uintptr_t
	mapEvacuatorIndexToMapAndMask(uintptr_t evacuatorIndex, uint64_t *evacuatorBitmask)
	{
		Debug_MM_true(evacuatorIndex < _evacuatorCount);
		*evacuatorBitmask = getEvacuatorBitMask(evacuatorIndex);
		return evacuatorIndex >> index_to_map_word_shift;
	}

	/* set evacuator bit in evacuator bitmap */
	bool
	testEvacuatorBit(uintptr_t evacuatorIndex, volatile uint64_t *bitmap)
	{
		Debug_MM_true(evacuatorIndex < _evacuatorCount);
		uint64_t evacuatorMask = 0;
		uintptr_t evacuatorMap = mapEvacuatorIndexToMapAndMask(evacuatorIndex, &evacuatorMask);
		return (evacuatorMask == (bitmap[evacuatorMap] & evacuatorMask));
	}

	/* calculate the number of active words in the evacuator bitmaps */
	MMINLINE uintptr_t countEvacuatorBitmapWords(uintptr_t *tailWords);

	/* test evacuator bitmap for all 0s (reliable only when caller holds controller mutex) */
	MMINLINE bool isEvacuatorBitmapEmpty(volatile uint64_t *bitmap);

	/* test evacuator bitmap for all 1s (reliable only when caller holds controller mutex) */
	MMINLINE bool isEvacuatorBitmapFull(volatile uint64_t *bitmap);

	/* fill evacuator bitmap with all 1s (reliable only when caller holds controller mutex) */
	MMINLINE void fillEvacuatorBitmap(volatile uint64_t * bitmap);

	/* set evacuator bit in evacuator bitmap */
	MMINLINE uint64_t setEvacuatorBit(uintptr_t evacuatorIndex, volatile uint64_t *bitmap);

	/* clear evacuator bit in evacuator bitmap */
	MMINLINE void clearEvacuatorBit(uintptr_t evacuatorIndex, volatile uint64_t *bitmap);

	/* test for object alignment */
	bool isObjectAligned(void *pointer) { return 0 == ((uintptr_t)pointer & (_objectAlignmentInBytes - 1)); }

	/* align to object size */
	uintptr_t alignToObjectSize(uintptr_t size) { return _extensions->objectModel.adjustSizeInBytes(size); }

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	/**
	 * Evacuator instances hold onto tenure whitelist contents between back-to-back nursery collections. These must
	 * be flushed before each global collection and should be flushed at collector shutdown.
	 *
	 * @param shutdown set false for global gc, true for collector shutdown
	 */
	void flushTenureWhitespace(bool shutdown);

	/**
	 * Called to initiate an evacuation and inform controller of number of GC threads that will participate (as
	 * evacuators) and heap layout.
	 *
	 * @param env environment for calling (master) thread
	 */
	virtual void masterSetupForGC(MM_EnvironmentStandard *env);

public:
	/**
	 * Controller delegates backout and remembered set to subclass
	 *
	 * TODO: MM_EvacuatorRememberedSet & MM_EvacuatorBackout
	 */
	virtual bool collectorStartup(MM_GCExtensionsBase* extensions);
	virtual void collectorShutdown(MM_GCExtensionsBase* extensions);
	virtual void scavengeRememberedSet(MM_EnvironmentStandard *env) = 0;
	virtual void pruneRememberedSet(MM_EnvironmentStandard *env) = 0;
	virtual void setBackOutFlag(MM_EnvironmentBase *env, BackOutState value) = 0;
	virtual void completeBackOut(MM_EnvironmentStandard *env) = 0;
	virtual void mergeThreadGCStats(MM_EnvironmentBase *env) = 0;
	virtual uintptr_t calculateTenureMask() = 0;

	/**
	 * Atomically test & set/reset a (public) evacuator flag.
	 *
	 * This method is also used with private flags used by controller and evacuators.
	 *
	 * @param flag the flag (bit) to set or reset
	 * @param value true to set the flag, false to reset
	 * @return true if the bit was previously set
	 */
	bool setEvacuatorFlag(uintptr_t flag, bool value);

	/**
	 * The controller maintains a bitset of public (defined by delegate) and private (defined by controller)
	 * flags that are used to communicate runtime conditions across all evacuators. The following methods are
	 * used to synchronize multicore views of the flags.
	 *
	 * The flags are all cleared at the start of each gc cycle.
	 */
	bool isEvacuatorFlagSet(uintptr_t flag) { return (flag == (_evacuatorFlags & flag)); }
	bool isAnyEvacuatorFlagSet(uintptr_t flags) { return (0 != (_evacuatorFlags & flags)); }
	bool areAllEvacuatorFlagsSet(uintptr_t flags) { return (flags == (_evacuatorFlags & flags)); }
	void resetEvacuatorFlags() { VM_AtomicSupport::set(&_evacuatorFlags, 0); }

	/**
	 * Get the number of GC threads dispatched for current gc cycle
	 */
	uintptr_t getEvacuatorThreadCount() { return _evacuatorCount; }

	/* these methods return accurate results only when caller holds the controller or evacuator mutex */
	bool isBoundEvacuator(uintptr_t evacuatorIndex) { return testEvacuatorBit(evacuatorIndex, _boundEvacuatorBitmap); }
	bool isStalledEvacuator(uintptr_t evacuatorIndex) { return isBoundEvacuator(evacuatorIndex) && testEvacuatorBit(evacuatorIndex, _stalledEvacuatorBitmap); }
	bool isResumingEvacuator(uintptr_t evacuatorIndex) { return isStalledEvacuator(evacuatorIndex) && testEvacuatorBit(evacuatorIndex, _resumingEvacuatorBitmap); }
	bool areAnyEvacuatorsStalled() { return (0 < _stalledEvacuatorCount); }

	/**
	 * Get the nearest neighboring bound evacuator, or wrap around and return identity if no other evacuators are bound
	 */
	MM_Evacuator *
	getNextEvacuator(MM_Evacuator *evacuator)
	{
		uintptr_t nextIndex = evacuator->getWorkerIndex();

		do {
			nextIndex = nextWorkerIndex(nextIndex);
		} while ((nextIndex != evacuator->getWorkerIndex()) && !isBoundEvacuator(nextIndex));

		return _evacuatorTask[nextIndex];
	}

	/**
	 * Evacuators call controller to assume/release exclusive controller access when completing or aborting scan cycle or completing an epoch
	 */
	void acquireController() { omrthread_monitor_enter(_controllerMutex); }

	void releaseController() { omrthread_monitor_exit(_controllerMutex); }

	/**
	 * Get global abort flag value. This is set if any evacuator raises an abort condition
	 *
	 * @return true if the evacuation has been aborted
	 */
	bool isAborting() { return isEvacuatorFlagSet(aborting); }

	/**
	 * Set global abort flag to true. This is set if any evacuator raises an abort condition
	 */
	bool setAborting(MM_Evacuator *abortingWorker);

	/**
	 * Parallel task wrapper calls this to bind worker thread to an evacuator instance at the beginning of a gc cycle.
	 *
	 * @param env the environment for the worker thread
	 * @return a pointer to the evacuator that is bound to the worker thread
	 */
	MM_Evacuator *bindWorker(MM_EnvironmentStandard *env);

	/**
	 * Parallel task wrapper calls this to unbind worker thread to an evacuator instance at the end of a gc cycle.
	 *
	 * @param env the environment for the worker thread
	 */
	void unbindWorker(MM_EnvironmentStandard *env);

	/**
	 * Get the memory subspace backing heap region.
	 *
	 * @return a pointer to the memory subspace
	 */
	MM_MemorySubSpace *getMemorySubspace(MM_Evacuator::EvacuationRegion region) { return _memorySubspace[region]; }

	/**
	 * Each evacuator task registers through controller to obtain a worker index that relates the evacuator
	 * to the controller. Each evacuator must call this before commencing evacuation.
	 *
	 * @param worker points to calling evacuator
	 * @param tenureMask pointer to controller's tenure mask
	 * @param heapBounds array of region bounds for evacuate, survivor, and tenure regions
	 * @param copiedBytesReportingDelta pointer to number of bytes to scan before reporting scan/copy progress to controller
	 */
	void startWorker(MM_Evacuator *worker, uintptr_t *tenureMask, uint8_t *heapBounds[][2], uint64_t *copiedBytesReportingDelta);

	/**
	 * Evacuator periodically reports scanning/copying progress to controller. Period is determined by
	 * bytes scanned delta set by controller. Last running (not stalled) reporting thread may end reporting
	 * epoch if not timesliced while accumulating summary scanned and copied byte counts across all
	 * evacuator threads.
	 *
	 * @param worker the reporting evacuator
	 * @param copied pointer to evacuator copied byte counter, which will be reset to 0
	 * @param scanned pointer to evacuator scanned byte counter, which will be reset to 0
	 */
	void reportProgress(MM_Evacuator *worker, uint64_t *copied, uint64_t *scanned);

	/**
	 * Calculate threshold for releasing work packets from outside copyspaces considering current aggregate
	 * volume of queued work and evacuator thread bandwidth.
	 *
	 * @param isRootScan true if the calling evacuator is evacuating root or clearable objects and has an empty scan stack
	 * @return the minimum number of unscanned bytes to accumulate in outside copyspaces before releasing a work packet
	 */
	uintptr_t calculateOptimalWorkPacketSize(bool isRootScan);

	/**
	 * Evacuator must call this before synchronizing with other evacuator threads through the MM_ParallelTask
	 * synchronization methods. This effectively removes the evacuator from the evacuation process until the
	 * evacuator thread continues after synchronizing.
	 *
	 * @param worker the synchronizing evacuator
	 * @param id callsite
	 */
	void waitToSynchronize(MM_Evacuator *worker, const char *id);

	/**
	 * Evacuator must call this after synchronizing with other evacuator threads through the MM_ParallelTask
	 * synchronization methods. This effectively restores the evacuator to resume participating in the
	 * evacuation process.
	 *
	 * @param worker the synchronizing evacuator
	 * @param startTime clock ticks at start of sync
	 * @param endTime clock ticks at end of sync
	 * @param id callsite
	 */
	void continueAfterSynchronizing(MM_Evacuator *worker, uint64_t startTime, uint64_t endTime, const char *id);

	/**
	 * Evacuator will notify controller of work whenever it adds to its own worklist. Controller will notify
	 * a stalled evacuator, if any are stalled,.and stalled evacuator may take the work from the working
	 * evacuator's worklist.
	 */
	void
	notifyOfWork()
	{
		if (areAnyEvacuatorsStalled()) {
			acquireController();
			omrthread_monitor_notify(_controllerMutex);
			releaseController();
		}
	}

	/**
	 * Evacuator will stall and wait on controller for work to arrive if unable to load work from its own worklist.
	 *
	 * Caller must have acquired controller mutex before and release it after the call
	 */
	void waitForWork() { omrthread_monitor_wait(_controllerMutex); }

	/**
	 * Evacuator calls controller when complete and ready to synchronize with other completing evacuators. All
	 * evacuator threads are synchronized when they are all stalled and have empty work queues. This join point
	 * marks the end of a heap scan. At this point the sums of the number of bytes copied and bytes scanned by
	 * each evacuator must be equal.
	 *
	 * @param worker the evacuator that is completed
	 * @param work if NULL, thread is still stalled, otherwise
	 * @return true if all evacuators have completed work, are stalled and waiting to end heap scan
	 */
	bool isWaitingToCompleteStall(MM_Evacuator *worker, MM_EvacuatorWorkPacket *work);

	/**
	 * Evacuator calls controller when it leaves stall loop after receiving work or after the last evacuator
	 * stalls and releases evacuators to complete a scan cycle. Evacuator threads are synchronized when they
	 * are all stalled with empty work queues. After synchronizing and leaving the stall loop evacuators with
	 * work continue to scan. Evacuators leave the stall loop without work only after the last evacuator to
	 * complete the stalled bitmap clears all evacuators to resume without work and complete the scan cycle.
	 *
	 * @param worker the evacuator that is trying to complete
	 * @param work work that is available to the evacuator
	 * @return the received work packet, or NULL if scan cycle is complete or aborting
	 */
	MM_EvacuatorWorkPacket *continueAfterStall(MM_Evacuator *worker, MM_EvacuatorWorkPacket *work);

	/**
	 * Evacuator calls this to determine whether there is scan work remaining in any evacuator's queue
	 *
	 * @return true if all material evacuated so far has been scanned
	 */
	bool hasCompletedScan() { return ((_copiedBytes[MM_Evacuator::survivor] + _copiedBytes[MM_Evacuator::tenure]) == _scannedBytes); }

	/**
	 * Evacuator calls this to get free space for inside (hierarchical) copying in specified destination memory space.
	 *
	 * @param worker the calling evacuator
	 * @param region the region (surivor or tenure) to obtain free space from
	 * @param length the (minimum) number of bytes of free space required
	 * @return a pointer to space allocated, which may be larger that the requested length
	 */
	MM_EvacuatorWhitespace *getInsideFreespace(MM_Evacuator *worker, MM_Evacuator::EvacuationRegion region, uintptr_t length);

	/**
	 * Evacuator calls this to get free space for outside (breadth-first) copying in specified destination memory space.
	 *
	 * @param worker the calling evacuator
	 * @param region the region (surivor or tenure) to obtain free space from
	 * @param remainder the number of bytes of free space remaining in the worker's copyspace for the region
	 * @param length the (minimum) number of bytes of free space required, a larger chunk may be allocated at controller discretion
	 * @return a pointer to space allocated, which may be larger that the requested length
	 */
	MM_EvacuatorWhitespace *getOutsideFreespace(MM_Evacuator *worker, MM_Evacuator::EvacuationRegion region, uintptr_t remainder, uintptr_t length);

	/* Get metrics from the most recently completed epoch in the current gc cycle */
	MM_EvacuatorHistory::Epoch *getEpoch() { return _history.epoch(); }

	/**
	 * Constructor
	 */
	MM_EvacuatorController(MM_EnvironmentBase *env)
		: MM_Collector()
		, _maxGCThreads(((MM_ParallelDispatcher *)env->getExtensions()->dispatcher)->threadCountMaximum())
		, _evacuatorTask(allocateEvacuatorArray(env, _maxGCThreads))
		, _evacuatorCount(0)
		, _controllerMutex(NULL)
		, _reporterMutex(NULL)
		, _boundEvacuatorBitmap(allocateEvacuatorBitmap(env, _maxGCThreads))
		, _stalledEvacuatorBitmap(allocateEvacuatorBitmap(env, _maxGCThreads))
		, _resumingEvacuatorBitmap(allocateEvacuatorBitmap(env, _maxGCThreads))
		, _evacuatorMask(allocateEvacuatorBitmap(env, _maxGCThreads))
		, _stalledEvacuatorCount(0)
		, _evacuatorIndex(0)
		, _evacuatorFlags(0)
		, _nextEpochCopiedBytesThreshold(0)
		, _copiedBytesReportingDelta(0)
		, _epochTimestamp(0)
#if defined(EVACUATOR_DEBUG) || defined(EVACUATOR_DEBUG_ALWAYS)
		, _collectorStartTime(0)
#endif /* defined(EVACUATOR_DEBUG) || defined(EVACUATOR_DEBUG_ALWAYS) */
		, _objectAlignmentInBytes(env->getExtensions()->objectModel.getObjectAlignmentInBytes())
		, _scannedBytes(0)
		, _finalDiscardedBytes(0)
		, _finalFlushedBytes(0)
		, _finalEvacuatedBytes(0)
		, _globalTenureFlushedBytes(0)
		, _extensions(env->getExtensions())
		, _dispatcher((MM_ParallelDispatcher *)_extensions->dispatcher)
		, _tenureMask(0)
		, _activeSubSpace(NULL)
		, _evacuateMemorySubSpace(NULL)
		, _survivorMemorySubSpace(NULL)
		, _tenureMemorySubSpace(NULL)
		, _evacuateSpaceBase(NULL)
		, _evacuateSpaceTop(NULL)
		, _survivorSpaceBase(NULL)
		, _survivorSpaceTop(NULL)
		, _omrVM(env->getOmrVM())
	{
		_typeId = __FUNCTION__;
		_copiedBytes[MM_Evacuator::survivor] = 0;
		_copiedBytes[MM_Evacuator::tenure] = 0;
	}

#if defined(EVACUATOR_DEBUG)
	uintptr_t sampleEvacuatorFlags() { return _evacuatorFlags; }
	volatile uint64_t *sampleStalledMap() { return _stalledEvacuatorBitmap; }
	volatile uint64_t *sampleResumingMap() { return _resumingEvacuatorBitmap; }
	void printEvacuatorBitmap(MM_EnvironmentBase *env, const char *label, volatile uint64_t *bitmap);
#endif /* defined(EVACUATOR_DEBUG) */
};

#endif /* EVACUATORCONTROLLER_HPP_ */
