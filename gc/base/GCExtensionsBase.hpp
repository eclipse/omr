/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(GCEXTENSIONSBASE_HPP_)
#define GCEXTENSIONSBASE_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "mmomrhook_internal.h"
#include "mmprivatehook_internal.h"
#include "modronbase.h"
#include "omr.h"

#include "AllocationStats.hpp"
#include "ArrayObjectModel.hpp"
#include "BaseVirtual.hpp"
#include "ExcessiveGCStats.hpp"
#include "Forge.hpp"
#include "GlobalGCStats.hpp"
#include "GlobalVLHGCStats.hpp"
#include "LargeObjectAllocateStats.hpp"
#include "MemoryHandle.hpp"
#include "MixedObjectModel.hpp"
#include "NUMAManager.hpp"
#include "OMRVMThreadListIterator.hpp"
#include "ObjectModel.hpp"
#include "ScavengerCopyScanRatio.hpp"
#include "ScavengerStats.hpp"
#include "SublistPool.hpp"

class MM_CardTable;
class MM_ClassLoaderRememberedSet;
class MM_CollectorLanguageInterface;
class MM_CompactGroupPersistentStats;
class MM_CompressedCardTable;
class MM_Configuration;
class MM_EnvironmentBase;
class MM_FrequentObjectsStats;
class MM_GlobalAllocationManager;
class MM_GlobalCollector;
class MM_Heap;
class MM_HeapMap;
class MM_HeapRegionManager;

class MM_InterRegionRememberedSet;
class MM_MemoryManager;
class MM_MemorySubSpace;
class MM_ParallelDispatcher;
#if defined(OMR_GC_OBJECT_MAP)
class MM_ObjectMap;
#endif /* defined(OMR_GC_OBJECT_MAP) */
class MM_ReferenceChainWalkerMarkMap;
class MM_RememberedSetCardBucket;
#if defined(OMR_GC_REALTIME)
class MM_RememberedSetSATB;
#endif /* defined(OMR_GC_REALTIME) */
class MM_Scavenger;
class MM_SizeClasses;
class MM_SparseVirtualMemory;
class MM_SweepHeapSectioning;
class MM_SweepPoolManager;
class MM_SweepPoolManagerAddressOrderedList;
class MM_SweepPoolManagerAddressOrderedListBase;
class MM_RealtimeGC;
class MM_VerboseManagerBase;
struct J9Pool;

namespace OMR {
namespace GC {
#if defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD)
class HeapRegionStateTable;
#endif /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */
} // namespace OMR
} // namespace GC

#if defined(OMR_ENV_DATA64)
#define MINIMUM_TLH_SIZE 768
#else
#define MINIMUM_TLH_SIZE 512
#endif /* defined(OMR_ENV_DATA64) */

/* The amount of work (array elements) to split when splitting array scanning. */
#define DEFAULT_ARRAY_SPLIT_MINIMUM_SIZE 512
#define DEFAULT_ARRAY_SPLIT_MAXIMUM_SIZE 16384

#define DEFAULT_SCAN_CACHE_MAXIMUM_SIZE (128 * 1024)
#define DEFAULT_SCAN_CACHE_MINIMUM_SIZE (8 * 1024)

#define NO_ESTIMATE_FRAGMENTATION 			0x0
#define LOCALGC_ESTIMATE_FRAGMENTATION 		0x1
#define GLOBALGC_ESTIMATE_FRAGMENTATION 	0x2

enum ExcessiveLevel {
	excessive_gc_normal = 0,
	excessive_gc_aggressive,
	excessive_gc_fatal,
	excessive_gc_fatal_consumed
};

enum BackOutState {
	backOutFlagCleared,		/* Normal state, no backout pending or in progress */
	backOutFlagRaised,		/* Backout pending */
	backOutStarted			/* Backout started */
};

/* Note:  These should be templates if DDR ever supports them (JAZZ 40487) */
class MM_UserSpecifiedParameterUDATA {
	/* Data Members */
private:
protected:
public:
	bool _wasSpecified; /**< True if this parameter was specified by the user, false means it is undefined */
	uintptr_t _valueSpecified; /**< The value specified by the user or undefined in _wasSpecified is false */

	/* Member Functions */
private:
protected:
public:
	MM_UserSpecifiedParameterUDATA()
		: _wasSpecified(false)
		, _valueSpecified(0)
	{
	}
};

class MM_UserSpecifiedParameterDouble {
	/* Data Members */
private:
protected:
public:
	bool _wasSpecified; /**< True if this parameter was specified by the user, false means it is undefined */
	double _valueSpecified; /**< The value specified by the user or undefined in _wasSpecified is false */

	/* Member Functions */
private:
protected:
public:
	MM_UserSpecifiedParameterDouble()
		: _wasSpecified(false)
		, _valueSpecified(0.0)
	{
	}
};

class MM_UserSpecifiedParameterBool {
	/* Data Members */
private:
protected:
public:
	bool _wasSpecified; /**< True if this parameter was specified by the user, false means it is undefined */
	bool _valueSpecified; /**< The value specified by the user or undefined in _wasSpecified is false */

	/* Member Functions */
private:
protected:
public:
	MM_UserSpecifiedParameterBool()
		: _wasSpecified(false)
		, _valueSpecified(false)
	{
	}
};

class MM_ConfigurationOptions : public MM_BaseNonVirtual
{
private:
public:
	MM_GCPolicy _gcPolicy; /**< gc policy (default or configured) */

	bool _forceOptionScavenge; /**< true if Scavenge option is forced in command line */
	bool _forceOptionConcurrentMark; /**< true if Concurrent Mark option is forced in command line */
	bool _forceOptionConcurrentSweep; /**< true if Concurrent Sweep option is forced in command line */
	bool _forceOptionLargeObjectArea; /**< true if Large Object Area option is forced in command line */
	bool _forceOptionWriteBarrierSATB; /**< Set with -Xgc:snapshotAtTheBeginningBarrier */

	MM_ConfigurationOptions()
		: MM_BaseNonVirtual()
		, _gcPolicy(gc_policy_undefined)
		, _forceOptionScavenge(false)
		, _forceOptionConcurrentMark(false)
		, _forceOptionConcurrentSweep(false)
		, _forceOptionLargeObjectArea(false)
		, _forceOptionWriteBarrierSATB(false)
	{
		_typeId = __FUNCTION__;
	}
};

class MM_GCExtensionsBase : public MM_BaseVirtual {
	/* Data Members */
private:
#if defined(OMR_GC_MODRON_SCAVENGER)
	void* _guaranteedNurseryStart; /**< lowest address guaranteed to be in the nursery */
	void* _guaranteedNurseryEnd; /**< highest address guaranteed to be in the nursery */

	bool _isRememberedSetInOverflow;

	volatile BackOutState _backOutState; /**< set if a thread is unable to copy an object due to lack of free space in both Survivor and Tenure */
	volatile bool _concurrentGlobalGCInProgress; /**< set to true if concurrent Global GC is in progress */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	bool debugConcurrentScavengerPageAlignment; /**< if true allows debug output prints for Concurrent Scavenger Page Alignment logic */
	uintptr_t concurrentScavengerPageSectionSize; /**< selected section size for Concurrent Scavenger Page */
	void *concurrentScavengerPageStartAddress; /**< start address for Concurrent Scavenger Page, UDATA_MAX if it is not initialized */
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

protected:
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	bool _compressObjectReferences;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
	OMR_VM* _omrVM;
	OMR::GC::Forge _forge;
	MM_GlobalCollector* _globalCollector; /**< The global collector for the system */
	uintptr_t lastGlobalGCFreeBytes;  /**< records the free memory size from after Global GC cycle */
#if defined(OMR_GC_OBJECT_MAP)
	MM_ObjectMap *_objectMap;
#endif /* defined(OMR_GC_OBJECT_MAP) */

public:
	bool _lazyCollectorInit; /**< Are we initializing without a collector? */

	MM_CollectorLanguageInterface* collectorLanguageInterface;

	void* _tenureBase; /**< Start address of Old subspace */
	uintptr_t _tenureSize; /**< Size of Old subspace in bytes */

	GC_ObjectModel objectModel; /**< generic object model for mixed and indexable objects */
	GC_MixedObjectModel mixedObjectModel; /**< object model for mixed objects */
	GC_ArrayObjectModel indexableObjectModel; /**< object model for arrays */

#if defined(OMR_GC_MODRON_SCAVENGER)
	MM_Scavenger *scavenger;
	void *_mainThreadTenureTLHRemainderBase;  /**< base and top pointers of the last unused tenure TLH copy cache, that will be loaded to thread env during main setup */
	void *_mainThreadTenureTLHRemainderTop;
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	J9Pool* environments;
	MM_ExcessiveGCStats excessiveGCStats;
#if defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME)
	MM_GlobalGCStats globalGCStats;
#endif /* defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME) */
#if defined(OMR_GC_MODRON_SCAVENGER)
	MM_ScavengerStats incrementScavengerStats; /**< scavengerStats for the current phase/increment; typically just used for reporting purposes */
	MM_ScavengerStats scavengerStats; /**< cumulative scavengerStats for all phases/increments (STW and concurrent) within a single cycle; typically used for various heursitics at the end of GC */
	MM_ScavengerCopyScanRatio copyScanRatio; /* Most recent estimate of ratio of aggregate slots copied to slots scanned in completeScan() */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#if defined(OMR_GC_VLHGC)
	MM_GlobalVLHGCStats globalVLHGCStats; /**< Global summary of all GC activity for VLHGC */
#endif /* defined(OMR_GC_VLHGC) */

#if defined(OMR_GC_CONCURRENT_SWEEP)
	/* Temporary move from the leaf implementation */
	bool concurrentSweep;
#endif /* defined(OMR_GC_CONCURRENT_SWEEP) */

	bool largePageWarnOnError;
	bool largePageFailOnError;
	bool largePageFailedToSatisfy;
#if defined(OMR_GC_DOUBLE_MAP_ARRAYLETS)
	bool isArrayletDoubleMapRequested;
	bool isArrayletDoubleMapAvailable;
#endif /* OMR_GC_DOUBLE_MAP_ARRAYLETS */
#if defined(OMR_GC_SPARSE_HEAP_ALLOCATION)
	bool isVirtualLargeObjectHeapRequested;
	bool isVirtualLargeObjectHeapEnabled;
#endif /* OMR_GC_SPARSE_HEAP_ALLOCATION */

	uintptr_t requestedPageSize;	/**< Memory page size for Object Heap */
	uintptr_t requestedPageFlags;	/**< Memory page flags for Object Heap */
	uintptr_t gcmetadataPageSize;	/**< Memory page size for GC Meta data */
	uintptr_t gcmetadataPageFlags;	/**< Memory page flags for GC Meta data */
	uintptr_t sparseHeapPageSize;	/**< Memory page size for Sparse Object Heap */
	uintptr_t sparseHeapPageFlags;	/**< Memory page flags for Sparse Object Heap */

#if defined(OMR_GC_MODRON_SCAVENGER)
	MM_SublistPool rememberedSet;
	uintptr_t oldHeapSizeOnLastGlobalGC;
	uintptr_t freeOldHeapSizeOnLastGlobalGC;
	float concurrentKickoffTenuringHeadroom; /**< percentage of free memory remaining in tenure heap. Used in conjunction with free memory to determine concurrent mark kickoff */
	float tenureBytesDeviationBoost; /**< boost factor for tenuring deviation used for concurrent mark kickoff math */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#if defined(OMR_GC_REALTIME)
	MM_RememberedSetSATB* sATBBarrierRememberedSet; /**< The snapshot at the beginning barrier remembered set used for the write barrier */
#endif /* defined(OMR_GC_REALTIME) */
	ModronLnrlOptions lnrlOptions;

	MM_OMRHookInterface omrHookInterface;
	MM_PrivateHookInterface privateHookInterface;

	void* heapBaseForBarrierRange0;
	uintptr_t heapSizeForBarrierRange0;

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
	void* shadowHeapBase; 	/* Read Barrier Verifier shadow heap base address */
	void* shadowHeapTop;	/* Read Barrier Verifier shadow heap base address */
	MM_MemoryHandle shadowHeapHandle; /* Read Barrier Verifier shadow heap Virtual Memory handle (descriptor) */
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

	bool doOutOfLineAllocationTrace;
	bool doFrequentObjectAllocationSampling; /**< Whether to track object allocations*/
	uintptr_t oolObjectSamplingBytesGranularity; /**< How often (in bytes) we do allocation sampling as tracked by per thread's local _oolTraceAllocationBytes. */
	uintptr_t objectSamplingBytesGranularity; /**< How often (in bytes) we do allocation sampling as tracked by per thread's local _traceAllocationBytes. */

	uintptr_t frequentObjectAllocationSamplingRate; /**< # bytes to sample / # bytes allocated */
	MM_FrequentObjectsStats* frequentObjectsStats;
	uint32_t frequentObjectAllocationSamplingDepth; /**< # of frequent objects we'd like to report */

	uint32_t estimateFragmentation; /**< Enable estimate fragmentation, NO_ESTIMATE_FRAGMENTATION, LOCALGC_ESTIMATE_FRAGMENTATION, GLOBALGC_ESTIMATE_FRAGMENTATION(default) */
	bool processLargeAllocateStats; /**< Enable process LargeObjectAllocateStats */
	uintptr_t largeObjectAllocationProfilingThreshold; /**< object size threshold above which the object is large enough for allocation profiling */
	uintptr_t largeObjectAllocationProfilingVeryLargeObjectThreshold; /**< object size threshold above which the object is large enough for recording precise size in allocation profiling */
	uintptr_t largeObjectAllocationProfilingVeryLargeObjectSizeClass; /**< index of sizeClass for minimum veryLargeEntry*/
	uint32_t largeObjectAllocationProfilingSizeClassRatio; /**< ratio of lower and upper boundary of a size class in large object allocation profiling */
	uint32_t largeObjectAllocationProfilingTopK; /**< number of most allocation size we want to track/report in large object allocation profiling */
	MM_FreeEntrySizeClassStats freeEntrySizeClassStatsSimulated; /**< snapshot of free memory status used for simulated allocator for fragmentation estimation */
	uintptr_t freeMemoryProfileMaxSizeClasses; /**< maximum number of sizeClass maintained for heap free memory profile (computed from SizeClassRatio) */

	volatile OMR_VMThread* gcExclusiveAccessThreadId; /**< thread token that represents the current "winning" thread for performing garbage collection */
	omrthread_monitor_t gcExclusiveAccessMutex; /**< Mutex used for acquiring gc priviledges as well as for signalling waiting threads that GC has been completed */

	J9Pool* _lightweightNonReentrantLockPool;
	omrthread_monitor_t _lightweightNonReentrantLockPoolMutex;

#if defined(OMR_GC_COMBINATION_SPEC)
	bool _isSegregatedHeap; /**< Are we using a segregated heap model */
	bool _isVLHGC; /**< Is balanced GC policy */
	bool _isMetronomeGC; /**< Is metronome GC policy */
	bool _isStandardGC; /**< Is it one of standard GC policy */
#endif /* defined(OMR_GC_COMBINATION_SPEC) */

	uintptr_t tlhMinimumSize;
	uintptr_t tlhMaximumSize;
	uintptr_t tlhInitialSize;
	uintptr_t tlhIncrementSize;
	uintptr_t tlhSurvivorDiscardThreshold; /**< below this size GC (Scavenger) will discard survivor copy cache TLH, if alloc not succeeded (otherwise we reuse memory for next TLH) */
	uintptr_t tlhTenureDiscardThreshold; /**< below this size GC (Scavenger) will discard tenure copy cache TLH, if alloc not succeeded (otherwise we reuse memory for next TLH) */

	MM_AllocationStats allocationStats; /**< Statistics for allocations. */
	uintptr_t bytesAllocatedMost;
	OMR_VMThread* vmThreadAllocatedMost;

	const char* gcModeString;
	uintptr_t splitFreeListSplitAmount; /**< The number of split freelists in heap (or in tenure for gencon) */
	bool splitFreeListAmountForced; /**< Flag to distinguish if splitFreeListAmount is externally enforced (for example, specified by command line) or determined heuristically */
	uintptr_t splitFreeListNumberChunksPrepared; /**< Used in MPSAOL postProcess. Shared for all MPSAOLs. Do not overwrite during postProcess for any MPSAOL. */
	bool enableHybridMemoryPool;

	bool largeObjectArea;
#if defined(OMR_GC_LARGE_OBJECT_AREA)
	typedef enum {
		METER_BY_SOA = 0,
		METER_BY_LOA,
		METER_DYNAMIC
	} ConcurrentMetering;

	uintptr_t largeObjectMinimumSize;
	double largeObjectAreaInitialRatio;
	double largeObjectAreaMinimumRatio;
	double largeObjectAreaMaximumRatio;
	bool debugLOAFreelist;
	bool debugLOAAllocate;
	int loaFreeHistorySize; /**< max size of _loaFreeRatioHistory array */
	uintptr_t lastGlobalGCFreeBytesLOA; /**< records the LOA free memory size from after Global GC cycle */
	ConcurrentMetering concurrentMetering;
	uintptr_t minimumContractionRatio;
#endif /* defined(OMR_GC_LARGE_OBJECT_AREA) */

	bool disableExplicitGC;
	uintptr_t heapAlignment;
	uintptr_t absoluteMinimumOldSubSpaceSize;
	uintptr_t absoluteMinimumNewSubSpaceSize;

	float darkMatterCompactThreshold; /**< Value used to trigger compaction when dark matter ratio reaches this percentage of memory pools memory*/
	float pageFragmentationCompactThreshold; /**< Enables compaction when page-fragmented memory and dark matter exceed this limit. The larger this number, the more memory can be fragmented before compact is triggered **/

	uintptr_t parSweepChunkSize;
	uintptr_t heapExpansionMinimumSize;
	uintptr_t heapExpansionMaximumSize;
	uintptr_t heapFreeMinimumRatioDivisor;
	uintptr_t heapFreeMinimumRatioMultiplier;
	uintptr_t heapFreeMaximumRatioDivisor;
	uintptr_t heapFreeMaximumRatioMultiplier;

	MM_UserSpecifiedParameterUDATA heapExpansionGCRatioThreshold; /**< max percentage of time spent in gc before expansion */
	MM_UserSpecifiedParameterUDATA heapContractionGCRatioThreshold; /**< min percentage of time spent in gc before contraction */

	uintptr_t heapExpansionStabilizationCount; /**< GC count required before the heap is allowed to expand due to excessvie time after last heap expansion */
	uintptr_t heapContractionStabilizationCount; /**< GC count required before the heap is allowed to contract due to excessvie time after last heap expansion */

	float heapSizeStartupHintConservativeFactor; /**< Use only a fraction of hints stored in SC */
	float heapSizeStartupHintWeightNewValue;		/**< Learn slowly by historic averaging of stored hints */
	bool useGCStartupHints; /**< Enabled/disable usage of heap sizing startup hints from Shared Cache */

	uintptr_t workpacketCount; /**< this value is ONLY set if -Xgcworkpackets is specified - otherwise the workpacket count is determined heuristically */
	uintptr_t packetListSplit; /**< the number of ways to split packet lists, set by command line option, or determined heuristically based on the number of GC threads */
	bool packetListSplitForced;  /**< Flag to distinguish if packetListSplit is externally enforced (for example, specified by command line) */
	uintptr_t markingArraySplitMaximumAmount; /**< maximum number of elements to split array scanning work in marking scheme */
	uintptr_t markingArraySplitMinimumAmount; /**< minimum number of elements to split array scanning work in marking scheme */

	bool rootScannerStatsEnabled; /**< Enable/disable recording of performance statistics for the root scanner.  Defaults to false. */
	bool rootScannerStatsUsed; /**< Flag that indicates if rootScannerStats are used for in the last increment (by any thread, for any of its roots) */

	/* bools and counters for -Xgc:fvtest options */
	bool fvtest_forceOldResize;
	uintptr_t fvtest_oldResizeCounter;
#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)
	uintptr_t fvtest_scanCacheCount;
#if defined(OMR_GC_MODRON_SCAVENGER)
	bool fvtest_forceScavengerBackout;
	uintptr_t fvtest_backoutCounter;
	bool fvtest_forcePoisonEvacuate; /**< if true poison Evacuate space with pattern at the end of scavenge */
	bool fvtest_forceNurseryResize;
	uintptr_t fvtest_nurseryResizeCounter;
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */
	bool fvtest_alwaysApplyOverflowRounding; /**< always round down the allocated heap as if overflow rounding were required */
	uintptr_t fvtest_forceExcessiveAllocFailureAfter; /**< force excessive GC to occur after this many global GCs */
	void* fvtest_verifyHeapAbove; /**< if non-NULL, will force start-up failure if any part of the heap is below this value */
	void* fvtest_verifyHeapBelow; /**< if non-NULL, will force start-up failure if any part of the heap is above this value */
#if defined(OMR_GC_VLHGC)
	bool fvtest_tarokVerifyMarkMapClosure; /**< True if the collector should verify that the new mark map defines a consistent and closed object graph after a GMP finishes creating it */
#endif /* defined(OMR_GC_VLHGC) */
	bool fvtest_disableInlineAllocation; /**< True if inline allocation should be disabled (i.e. force out-of-line paths) */

	uintptr_t fvtest_forceSweepChunkArrayCommitFailure; /**< Force failure at Sweep Chunk Array commit operation */
	uintptr_t fvtest_forceSweepChunkArrayCommitFailureCounter; /**< Force failure at Sweep Chunk Array commit operation counter */
#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
	uintptr_t fvtest_enableReadBarrierVerification; /**< Forces failure at all direct memory read sites */
	uintptr_t fvtest_enableMonitorObjectsReadBarrierVerification; /**< Forces failure at all direct memory read sites for monitor slot objects */
	uintptr_t fvtest_enableClassStaticsReadBarrierVerification; /**< Forces failure at all direct memory read sites for class statics */
	uintptr_t fvtest_enableJNIGlobalWeakReadBarrierVerification; /**< Forces failure at all direct memory read sites for JNI Global weak references */
	uintptr_t fvtest_enableHeapReadBarrierVerification; /**< Forces failure at all direct memory read sites for heap references */
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

	uintptr_t fvtest_forceMarkMapCommitFailure; /**< Force failure at Mark Map commit operation */
	uintptr_t fvtest_forceMarkMapCommitFailureCounter; /**< Force failure at Mark Map commit operation counter */
	uintptr_t fvtest_forceMarkMapDecommitFailure; /**< Force failure at Mark Map decommit operation */
	uintptr_t fvtest_forceMarkMapDecommitFailureCounter; /**< Force failure at Mark Map decommit operation counter */

	uintptr_t fvtest_forceReferenceChainWalkerMarkMapCommitFailure; /**< Force failure at Reference Chain Walker Mark Map commit operation */
	uintptr_t fvtest_forceReferenceChainWalkerMarkMapCommitFailureCounter; /**< Force failure at Reference Chain Walker Mark Map commit operation counter */

	uintptr_t fvtest_forceCopyForwardHybridRatio; /**< Force to run CopyForward Hybrid mode value = 1-100 the percentage of non evacuated eden regions */
	uintptr_t softMx; /**< set through -Xsoftmx, depending on GC policy this number might differ from available heap memory, use MM_Heap::getActualSoftMxSize for calculations */

#if defined(OMR_GC_BATCH_CLEAR_TLH)
	uintptr_t batchClearTLH;
#endif /* defined(OMR_GC_BATCH_CLEAR_TLH) */
	omrthread_monitor_t gcStatsMutex;
	uintptr_t gcThreadCount; /**< Initial number of GC threads - chosen default or specified in java options*/
	bool gcThreadCountSpecified; /**< true if number of GC threads is specified in command line options. */
	bool gcThreadCountForced; /**< true if user forced a fixed number of GC threads. Default is false, but a command line option could set it if not wanting adaptive threading */
	uintptr_t dispatcherHybridNotifyThreadBound; /**< Bound for determining hybrid notification type (Individual notifies for count < MIN(bound, maxThreads/2), otherwise notify_all) */

#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)
	enum ScavengerScanOrdering {
		OMR_GC_SCAVENGER_SCANORDERING_NONE = 0,
		OMR_GC_SCAVENGER_SCANORDERING_BREADTH_FIRST,
		OMR_GC_SCAVENGER_SCANORDERING_DYNAMIC_BREADTH_FIRST,
		OMR_GC_SCAVENGER_SCANORDERING_HIERARCHICAL,
	};
	ScavengerScanOrdering scavengerScanOrdering; /**< scan ordering in Scavenger */
	/* Start of options relating to dynamicBreadthFirstScanOrdering */
	uintptr_t gcCountBetweenHotFieldSort;
	uintptr_t gcCountBetweenHotFieldSortMax;
	bool adaptiveGcCountBetweenHotFieldSort;
	bool depthCopyTwoPaths;
	bool depthCopyThreePaths;
	bool alwaysDepthCopyFirstOffset;
	bool allowPermanantHotFields;
	bool hotFieldResettingEnabled;
	uintptr_t maxConsecutiveHotFieldSelections;
	uintptr_t gcCountBetweenHotFieldReset;
	uintptr_t depthCopyMax;
	uint32_t maxHotFieldListLength;
	uintptr_t minCpuUtil;
	/* End of options relating to dynamicBreadthFirstScanOrdering */
#if defined(OMR_GC_MODRON_SCAVENGER)
	uintptr_t scvTenureRatioHigh;
	uintptr_t scvTenureRatioLow;
	uintptr_t scvTenureFixedTenureAge; /**< The tenure age to use for the Fixed scavenger tenure strategy. */
	uintptr_t scvTenureAdaptiveTenureAge; /**< The tenure age to use for the Adaptive scavenger tenure strategy. */
	double scvTenureStrategySurvivalThreshold; /**< The survival threshold (from 0.0 to 1.0) used for deciding to tenure particular ages. */
	bool scvTenureStrategyFixed; /**< Flag for enabling the Fixed scavenger tenure strategy. */
	bool scvTenureStrategyAdaptive; /**< Flag for enabling the Adaptive scavenger tenure strategy. */
	bool scvTenureStrategyLookback; /**< Flag for enabling the Lookback scavenger tenure strategy. */
	bool scvTenureStrategyHistory; /**< Flag for enabling the History scavenger tenure strategy. */
	bool scavengerEnabled;
	bool scavengerRsoScanUnsafe;
	uintptr_t cacheListSplit; /**< the number of ways to split scanCache lists, set by command line option, or determined heuristically based on the number of GC threads */
	bool cacheListSplitForced;/**< Flag to distinguish if cacheList is externally enforced (for example, specified by command line) */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	bool softwareRangeCheckReadBarrier; /**< enable software read barrier instead of hardware guarded loads when running with CS, complimentary to concurrentScavengerHWSupport with CS active */
	bool softwareRangeCheckReadBarrierForced; /**< true if usage of softwareRangeCheckReadBarrier is requested explicitly */
	bool concurrentScavenger; /**< CS enabled/disabled flag */
	bool concurrentScavengerForced; /**< set to true if CS is requested (by cmdline option), but there are more checks to do before deciding whether the request is to be obeyed */
	bool concurrentScavengerHWSupport; /**< set to true if CS runs with HW support, complimentary to softwareRangeCheckReadBarrier with CS active */
	uintptr_t concurrentScavengerBackgroundThreads; /**< number of background GC threads during concurrent phase of Scavenge */
	bool concurrentScavengerBackgroundThreadsForced; /**< true if concurrentScavengerBackgroundThreads set via command line option */
	uintptr_t concurrentScavengerSlack; /**< amount of bytes added on top of avearge allocated bytes during concurrent cycle, in calcualtion for survivor size */
	float concurrentScavengerAllocDeviationBoost; /**< boost factor for allocate rate and its deviation, used for tilt calcuation in Concurrent Scavenger */
	bool concurrentScavengeExhaustiveTermination; /**< control flag to enable/disable concurrent phase termination optimization using involing async mutator callbacks */
#endif	/* defined(OMR_GC_CONCURRENT_SCAVENGER) */
	uintptr_t scavengerFailedTenureThreshold;
	uintptr_t maxScavengeBeforeGlobal;
	uintptr_t scvArraySplitMaximumAmount; /**< maximum number of elements to split array scanning work in the scavenger */
	uintptr_t scvArraySplitMinimumAmount; /**< minimum number of elements to split array scanning work in the scavenger */
	uintptr_t scavengerScanCacheMaximumSize; /**< maximum size of scan and copy caches before rounding, zero (default) means calculate them */
	uintptr_t scavengerScanCacheMinimumSize; /**< minimum size of scan and copy caches before rounding, zero (default) means calculate them */
	bool tiltedScavenge;
	bool debugTiltedScavenge;
	double survivorSpaceMinimumSizeRatio;
	double survivorSpaceMaximumSizeRatio;
	double tiltedScavengeMaximumIncrease;
	double scavengerCollectorExpandRatio; /**< the ratio of _avgTenureBytes we use to expand when a collectorAllocate() fails */
	uintptr_t scavengerMaximumCollectorExpandSize; /**< the maximum amount by which we will expand when a collectorAllocate() fails */
	bool dynamicNewSpaceSizing;
	bool debugDynamicNewSpaceSizing;
	bool dnssAvoidMovingObjects;

	MM_UserSpecifiedParameterDouble dnssExpectedRatioMinimum; /**< When the gc ratio for new/nursery space is below this value, new/nursery space should contract */
	MM_UserSpecifiedParameterDouble dnssExpectedRatioMaximum; /**< When the gc ratio for new/nursery space is above this value, new/nursery space should expand */

	double dnssWeightedTimeRatioFactorIncreaseSmall;
	double dnssWeightedTimeRatioFactorIncreaseMedium;
	double dnssWeightedTimeRatioFactorIncreaseLarge;
	double dnssWeightedTimeRatioFactorDecrease;
	double dnssMaximumExpansion;
	double dnssMaximumContraction;
	double dnssMinimumExpansion;
	double dnssMinimumContraction;
	bool enableSplitHeap; /**< true if we are using gencon with -Xgc:splitheap (we will fail to boostrap if we can't allocate both ranges) */
	double aliasInhibitingThresholdPercentage; /**< percentage of threads that can be blocked before copy cache aliasing is inhibited (set through aliasInhibitingThresholdPercentage=) */

	/* Start of variables relating to Adaptive Threading */
	bool adaptiveGCThreading; /**< Flag to indicate whether the Scavenger Adaptive Threading Optimization is enabled*/
	float adaptiveThreadingSensitivityFactor; /**<  Used by Adaptive Model to determine sensitivity/tolerance to stalling, higher number translates to less stall being tolerated (set through adaptiveThreadingSensitivityFactor=) */
	float adaptiveThreadingWeightActiveThreads; /**< Weight given to current active threads when averaging projected threads with current active threads (set through adaptiveThreadingWeightActiveThreads=) */
	float adaptiveThreadBooster; /**< Used to boost calculated thread count, gives opportunity for low thread count to grow. */
	/* End of variables relating to Adaptive Threading */

	enum HeapInitializationSplitHeapSection {
		HEAP_INITIALIZATION_SPLIT_HEAP_UNKNOWN = 0,
		HEAP_INITIALIZATION_SPLIT_HEAP_TENURE,
		HEAP_INITIALIZATION_SPLIT_HEAP_NURSERY,
	};

	HeapInitializationSplitHeapSection splitHeapSection; /**< Split Heap section to be requested */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */
	double globalMaximumContraction; /**< maximum percentage of committed global heap which can contract in one GC cycle (set through -Xgc:globalMaximumContraction=) */
	double globalMinimumContraction; /**< minimum percentage of committed global heap which can contract in one GC cycle (set through -Xgc:globalMinimumContraction=) */

	/* global variables for excessiveGC functionality */
	MM_UserSpecifiedParameterBool excessiveGCEnabled; /**< should we check for excessiveGC? (set through -XdisableExcessiveGC and -XenableExcessiveGC) */
	bool isRecursiveGC; /**< is the current executing gc a result of another gc (ie: scavenger triggering a global collect) */
	bool didGlobalGC; /**< has a global gc occurred in the current gc (possibly as a result of a recursive gc) */
	ExcessiveLevel excessiveGCLevel;
	float excessiveGCnewRatioWeight;
	uintptr_t excessiveGCratio;
	float excessiveGCFreeSizeRatio;

	MM_Heap* heap;
	MM_SparseVirtualMemory *largeObjectVirtualMemory; /**< Virtual memory for large objects (objectSize > arrayletLeafSize). Live large objects are committed to this separate virtual memory space when isVirtualLargeObjectHeapEnabled is true */
	MM_HeapRegionManager* heapRegionManager; /**< The heap region manager used to view the heap as regions of memory */
	MM_MemoryManager* memoryManager; /**< memory manager used to access to virtual memory instances */
	uintptr_t aggressive;
	MM_SweepHeapSectioning* sweepHeapSectioning; /**< Reference to the SweepHeapSectioning to Compact can share the backing store */

#if defined(OMR_GC_MODRON_COMPACTION)
	uintptr_t compactOnGlobalGC;
	uintptr_t noCompactOnGlobalGC;
	uintptr_t compactOnSystemGC;
	uintptr_t nocompactOnSystemGC;
	bool compactToSatisfyAllocate;
#endif /* defined(OMR_GC_MODRON_COMPACTION) */

	bool payAllocationTax;

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	bool concurrentMark;
	bool concurrentKickoffEnabled;
	double concurrentSlackFragmentationAdjustmentWeight; /**< weight(from 0.0 to 5.0) used for calculating free tenure space (how much percentage of the fragmentation need to remove from freeBytes) */
	bool debugConcurrentMark;
	bool optimizeConcurrentWB;
	uintptr_t concurrentLevel;
	uintptr_t concurrentBackground;
	uintptr_t concurrentSlack; /**< number of bytes to add to the concurrent kickoff threshold buffer */
	uintptr_t cardCleanPass2Boost;
	uintptr_t cardCleaningPasses;

	UDATA fvtest_concurrentCardTablePreparationDelay; /**< Delay for concurrent card table preparation in milliseconds */

	UDATA fvtest_forceConcurrentTLHMarkMapCommitFailure; /**< Force failure at Concurrent TLH Mark Map commit operation */
	UDATA fvtest_forceConcurrentTLHMarkMapCommitFailureCounter; /**< Force failure at Concurrent TLH Mark Map commit operation counter */
	UDATA fvtest_forceConcurrentTLHMarkMapDecommitFailure; /**< Force failure at Concurrent TLH Mark Map decommit operation */
	UDATA fvtest_forceConcurrentTLHMarkMapDecommitFailureCounter; /**< Force failure at Concurrent TLH Mark Map decommit operation  counter */
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

	UDATA fvtest_forceCardTableCommitFailure; /**< Force failure at Card Table commit operation */
	UDATA fvtest_forceCardTableCommitFailureCounter; /**< Force failure at Card Table commit operation  counter */
	UDATA fvtest_forceCardTableDecommitFailure; /**< Force failure at Card Table decommit operation */
	UDATA fvtest_forceCardTableDecommitFailureCounter; /**< Force failure at Card Table decommit operation  counter */

	MM_ParallelDispatcher* dispatcher;

#if defined(J9VM_OPT_CRIU_SUPPORT)
	uintptr_t checkpointGCthreadCount;
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

	MM_CardTable* cardTable;

	/* Begin command line options temporary home */
	uintptr_t memoryMax;
	uintptr_t initialMemorySize;
	uintptr_t minNewSpaceSize;
	uintptr_t newSpaceSize;
	uintptr_t maxNewSpaceSize;
	uintptr_t minOldSpaceSize;
	uintptr_t oldSpaceSize;
	uintptr_t maxOldSpaceSize;
	uintptr_t allocationIncrement;
	uintptr_t fixedAllocationIncrement;
	uintptr_t lowMinimum;
	uintptr_t allowMergedSpaces;
	uintptr_t maxSizeDefaultMemorySpace;
	bool allocationIncrementSetByUser;
	/* End command line options temporary home */

	uintptr_t overflowSafeAllocSize;

	uint64_t usablePhysicalMemory; /**< Physical memory available to the process */

#if defined(OMR_GC_REALTIME)
	/* Parameters */
	uintptr_t RTC_Frequency;
	uintptr_t itPeriodMicro;
	uintptr_t hrtPeriodMicro;
	uintptr_t debugWriteBarrier;
	uintptr_t timeWindowMicro;
	uintptr_t beatMicro;
	bool overrideHiresTimerCheck; /**< ignore the values returned from clokc_getres if this value is true */
	uintptr_t targetUtilizationPercentage;
	uintptr_t gcTrigger; // start gc when bytes used exceeds gcTrigger
	uintptr_t gcInitialTrigger; // start gc when bytes used exceeds gcTrigger
	uintptr_t headRoom; // at end of GC, reset gcTrigger to OMR_MAX(gcInitialTrigger, usedMemory + headRoom)
	bool synchronousGCOnOOM;
	bool extraYield;
	/* Global variables */
	MM_RealtimeGC* realtimeGC;
	bool fixHeapForWalk; /**< configuration flag set by command line option or GC Check onload */
	uintptr_t minArraySizeToSetAsScanned;
	uintptr_t overflowCacheCount; /**< How many entries should there be in the environments local overflow cache */
#endif /* defined(OMR_GC_REALTIME) */

#if defined(OMR_GC_REALTIME)
	bool concurrentSweepingEnabled; /**< if this is set, the sweep phase of GC will be run concurrently */
	bool concurrentTracingEnabled; /**< if this is set, tracing will run concurrently */
#endif /* defined(OMR_GC_REALTIME) */

	bool instrumentableAllocateHookEnabled;

	MM_HeapMap* previousMarkMap; /**< the previous valid mark map. This can be used to walk marked objects in regions which have _markMapUpToDate set to true */

	MM_GlobalAllocationManager* globalAllocationManager; /**< Used for attaching threads to AllocationContexts */

#if defined(OMR_GC_REALTIME) || defined(OMR_GC_SEGREGATED_HEAP)
	uintptr_t managedAllocationContextCount; /**< The number of allocation contexts which will be instantiated and managed by the GlobalAllocationManagerRealtime (currently 2*cpu_count) */
#endif /* defined(OMR_GC_REALTIME) || defined(OMR_GC_SEGREGATED_HEAP) */

#if defined(OMR_GC_SEGREGATED_HEAP)
	MM_SizeClasses* defaultSizeClasses;
#endif /* defined(OMR_GC_SEGREGATED_HEAP) */

#if defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD)
	OMR::GC::HeapRegionStateTable *heapRegionStateTable;
#endif /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */

/* OMR_GC_REALTIME (in for all -- see 82589) */
	uint32_t distanceToYieldTimeCheck; /**< Number of condYield that can be skipped before actual checking for yield, when the quanta time has been relaxed */
	uintptr_t traceCostToCheckYield; /**< tracing cost (in number of objects marked and pointers scanned) after we try to yield */
	uintptr_t sweepCostToCheckYield; /**< weighted count of free chunks/marked objects before we check yield in sweep small loop */
	uintptr_t splitAvailableListSplitAmount; /**< Number of split available lists per size class, per defragment bucket */
	uint32_t newThreadAllocationColor;
	uintptr_t minimumFreeEntrySize;
	uintptr_t arrayletsPerRegion;
	uintptr_t verbose; // Accessability from mmparse
	uintptr_t debug;
	uintptr_t allocationTrackerMaxTotalError; /**< The total maximum desired error for the free bytes approximation, the larger the number, the lower the contention and vice versa */
	uintptr_t allocationTrackerMaxThreshold; /**< The maximum threshold for a single allocation tracker */
	uintptr_t allocationTrackerFlushThreshold; /**< The flush threshold to be used for all allocation trackers, this value is adjusted every time a new thread is created/destroyed */
	/* TODO: These variables should also be used for TLHs */
	uintptr_t allocationCacheMinimumSize;
	uintptr_t allocationCacheMaximumSize;
	uintptr_t allocationCacheInitialSize;
	uintptr_t allocationCacheIncrementSize;
	bool nonDeterministicSweep;
/* OMR_GC_REALTIME (in for all) */

	MM_ConfigurationOptions configurationOptions; /**< holds the options struct, used during startup for selecting a Configuration */
	MM_Configuration* configuration; /**< holds the Configuration selected during startup */

	MM_VerboseManagerBase* verboseGCManager;

	uintptr_t verbosegcCycleTime;
	bool verboseExtensions;
	bool verboseNewFormat; /**< a flag, enabled by -XXgc:verboseNewFormat, to enable the new verbose GC format */
	bool bufferedLogging; /**< Enabled by -Xgc:bufferedLogging.  Use buffered filestreams when writing logs (e.g. verbose:gc) to a file */

	uintptr_t lowAllocationThreshold; /**< the lower bound of the allocation threshold range */
	uintptr_t highAllocationThreshold; /**< the upper bound of the allocation threshold range */
	bool disableInlineCacheForAllocationThreshold; /**< true if inline allocates fall within the allocation threshold*/
	bool disableInlineAllocationForSamplingBytesGranularity; /**< true if inline allocation should be "disabled" for SamplingBytesGranularity */
	uintptr_t heapCeiling; /**< the highest point in memory where objects can be addressed (used for the -Xgc:lowMemHeap option) */

	enum HeapInitializationFailureReason {
		HEAP_INITIALIZATION_FAILURE_REASON_NO_ERROR = 0,
		HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_INSTANTIATE_HEAP,
		HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_INSTANTIATE_SPLIT_HEAP_OLD_SPACE,
		HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_INSTANTIATE_SPLIT_HEAP_NEW_SPACE,
		HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_INSTANTIATE_SPLIT_HEAP_GEOMETRY,
		HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_ALLOCATE_LOW_MEMORY_RESERVE,
		HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_SATISFY_REQUESTED_PAGE_SIZE,
		HEAP_INITIALIZATION_FAILURE_REASON_METRONOME_DOES_NOT_SUPPORT_4BIT_SHIFT,
	};
	HeapInitializationFailureReason heapInitializationFailureReason; /**< Error code provided additional information about heap initialization failure */
	bool scavengerAlignHotFields; /**< True if the scavenger is to check the hot field description for an object in order to better cache align it when tenuring (enabled with the -Xgc:hotAlignment option) */
	uintptr_t suballocatorInitialSize; /**< the initial chunk size in bytes for the J9Heap suballocator (enabled with the -Xgc:suballocatorInitialSize option) */
	uintptr_t suballocatorCommitSize; /**< the commit size in bytes for the J9Heap suballocator (enabled with the -Xgc:suballocatorCommitSize option) */

#if defined(OMR_GC_COMPRESSED_POINTERS)
	bool shouldAllowShiftingCompression; /**< temporary option to enable compressed reference scaling by shifting pointers */
	bool shouldForceSpecifiedShiftingCompression; /**< temporary option to enable forcedShiftingCompressionAmount */
	uintptr_t forcedShiftingCompressionAmount; /**< temporary option to force compressed reference scaling to use this as the shifted value (typically 0-3 in current usage) */
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */

	uintptr_t preferredHeapBase; /**< the preferred heap base for heap allocated using virtual memory */

	uintptr_t currentEnvironmentCount; /**< The current count of active environments, aka: running threads */
	uintptr_t regionSize; /**< The size, in bytes, of a fixed-size table-backed region of the heap (does not apply to AUX regions) */
	MM_NUMAManager _numaManager; /**< The object which abstracts the details of our NUMA support so that the GCExtensions and the callers don't need to duplicate the support to interpret our intention */
	bool numaForced; /**< if true, specifies if numa is disabled or enabled (actual value stored in NUMA Manager) by command line option */

	bool padToPageSize;

	bool fvtest_disableExplictMainThread; /**< Test option to disable creation of explicit main GC thread */

#if defined(OMR_GC_VLHGC)
	bool tarokDebugEnabled; /**< True if we want to perform additional checks and freed memory poisoning which aid in debugging Tarok problems  */
	uintptr_t tarokGlobalMarkIncrementTimeMillis; /**< The time period in millisecond a Global Mark increment is allowed to run (as set by the user, normally dynamic)*/
	uintptr_t fvtest_tarokForceNUMANode; /**< The NUMA node to force the heap onto (UDATA_MAX => no force, 0 => interleaved, >0 => specific node) */
	uintptr_t fvtest_tarokFirstContext; /**< The allocation context number to use first, when associating the first thread (assignments will proceed, as a round robin, from this number).  Defaults to 0 */
	bool tarokEnableScoreBasedAtomicCompact; /**< True if atomic compact does use score based compact region selection heuristic */
	uintptr_t tarokIdealEdenMinimumBytes; /**< The ideal size of the eden space, in bytes, when the heap is at its -Xms size */
	uintptr_t tarokIdealEdenMaximumBytes; /**< The ideal size of the eden space, in bytes, when the heap is at its -Xmx size */
	bool tarokEnableIncrementalGMP; /**< True if we want to perform GMP work as a series of increments during the run.  (set to false if we should rely on OOM global collections to perform the GMP) */
	MM_UserSpecifiedParameterUDATA tarokNurseryMaxAge; /**< The maximum age that a region will be before it is excluded from a partial garbage collection */
	uintptr_t tarokRememberedSetCardListMaxSize; /* The maximum size in entries of RememberedSetCardList per region */
	uintptr_t tarokRememberedSetCardListSize; /* The average (allocated) size in entries of RememberedSetCardList per region */
	uintptr_t tarokPGCtoGMPNumerator; /* The numerator of the PGC:GMP ratio */
	uintptr_t tarokPGCtoGMPDenominator; /* The denominator of the PGC:GMP ratio */
	uintptr_t tarokGMPIntermission; /** The delay between GMP cycles, specified as the number of GMP increments to skip */
	bool tarokAutomaticGMPIntermission; /** Should the delay between GMP cycles be automatic, or as specified in tarokGMPIntermission? */
	uintptr_t tarokRegionMaxAge; /**< Maximum age a region can be before it will no longer have its age incremented after a PGC (saturating age) */
	uintptr_t tarokKickoffHeadroomInBytes; /**< extra bytes reserved for survivor set, in case of sudden changes of survivor rate. Used in calculation to predict GMP kickoff */
	bool 	  tarokForceKickoffHeadroomInBytes; /** true if user specifies tarokKickoffHeadroomInBytes via -XXgc:tarokKickoffHeadroomInBytes= */
	uint32_t tarokKickoffHeadroomRegionRate; /**< used by calculating tarokKickoffHeadroomInBytes, the percentage of the free memory, range: 0(0%)<=the rate<=50(50%) , default=2 (2%)  */
	MM_RememberedSetCardBucket* rememberedSetCardBucketPool; /* GC thread local pools of RS Card Buckets for each Region (its Card List) */
	bool tarokEnableDynamicCollectionSetSelection; /**< Enable dynamic selection of regions to include in the collection set that reside outside of the nursery */
	uintptr_t tarokDynamicCollectionSetSelectionAbsoluteBudget; /**< Number of budgeted regions to dynamically select for PGC collection (outside of the required nursery set) */
	double tarokDynamicCollectionSetSelectionPercentageBudget; /**< Percentage increase of nursery region count to use as dynamically selected regions for PGC */
	uintptr_t tarokCoreSamplingAbsoluteBudget; /**< Number of budgeted regions to select for core sampling in a PGC collection (outside of the required nursery set) */
	double tarokCoreSamplingPercentageBudget; /**< Percentage increase of nursery region count to use as core sampling selected regions for PGC */
	void* tarokTgcSetSelectionDataTable; /**< (TGC USE ONLY!) Table containing all dynamic and core sampling set selection information */
	bool tarokTgcEnableRememberedSetDuplicateDetection; /** (TGC USE ONLY!) True if we want to enable duplicate card stats reported by TGC for RSCL */
	bool tarokPGCShouldCopyForward; /**< True if we want to allow PGC increments to reclaim memory using copy-forward (default is true) */
	bool tarokPGCShouldMarkCompact; /**< True if we want to allow PGC increments to reclaim memory using compact (default is true) and require a corresponding mark operation */
	MM_InterRegionRememberedSet* interRegionRememberedSet; /**< The remembered set abstraction to be used to track inter-region references found while processing this cycle */
	bool tarokEnableStableRegionDetection; /**< Enable overflowing RSCSLs for stable regions */
	double tarokDefragmentEmptinessThreshold; /**< Emptiness (freeAndDarkMatter/regionSize) for a region to be considered as a target for defragmentation (used for stable region detection and region de-fragmentation selection) */
	bool tarokAttachedThreadsAreCommon; /**< True if we want to associate all common threads with the "common context" which is otherwise only reserved for regions which the collector has identified as common to all nodes ("common" regions are still moved to this context, no matter the value of this flag) */
	double tarokCopyForwardFragmentationTarget; /**< The fraction of discarded space targeted in each copy-forward collection. The actual amount may be lower or higher than this fraction. */
	bool tarokEnableCardScrubbing; /**< Set if card scrubbing in GMPs is enabled (default is true) */
	bool tarokEnableConcurrentGMP; /**< Set if the GMP should attempt to accomplish work concurrently, where possible.  False implies GMP work will only be done in the stop-the-world increments */
	MM_CompactGroupPersistentStats* compactGroupPersistentStats; /**< The global persistent stats indexed by compact group number */
	MM_ClassLoaderRememberedSet* classLoaderRememberedSet; /**< The remembered set abstraction to be used to track references from instances to the class loaders containing their defining class */
	bool tarokEnableIncrementalClassGC; /**< Enable class unloading during partial garbage collection increments */
	bool tarokEnableCompressedCardTable; /**< Enable usage of Compressed Card Table (Summary) */
	MM_CompressedCardTable* compressedCardTable; /**< The pointer to Compressed Card Table */
	bool tarokEnableLeafFirstCopying; /**< Enable copying of leaf children immediately after parent is copied in CopyForwardScheme */
	uint64_t tarokMaximumAgeInBytes; /**< Maximum age in bytes for bytes-based-allocated aging system */
	uint64_t tarokMaximumNurseryAgeInBytes; /**< Maximum Nursery Age in bytes for bytes-based-allocated aging system */
	bool tarokAllocationAgeEnabled; /**< Enable Allocation-based aging system */
	uintptr_t tarokAllocationAgeUnit; /**< base unit for allocation-based aging system */
	double tarokAllocationAgeExponentBase; /**< allocation-based aging system exponent base */
	bool tarokUseProjectedSurvivalCollectionSet; /**< True if we should use a collection set based on the projected survival rate of regions*/
	uintptr_t tarokWorkSplittingPeriod; /**< The number of objects which must be scanned between each check that the depth-first copy-forward implementation makes to see if it should push work out to other threads */
	MM_UserSpecifiedParameterUDATA tarokMinimumGMPWorkTargetBytes; /**< Minimum used for GMP work targets.  This avoids the low-scan-rate -> low GMP work target -> low scan-rate feedback loop. */
	double tarokConcurrentMarkingCostWeight; /**< How much we weigh concurrentMarking into our GMP scan time cost calculations */
	bool tarokAutomaticDefragmentEmptinessThreshold; /**< Whether we should use the automatically derived value for tarokDefragmentEmptinessThreshold or not */
	bool tarokEnableCopyForwardHybrid; /**< Enable CopyForward Hybrid mode */
	enum ReserveRegions {
		RESERVE_REGIONS_NO = 0,
		RESERVE_REGIONS_MOST_ALLOCATABLE,
		RESERVE_REGIONS_MOST_FREE
	};
	ReserveRegions tarokReserveRegionsFromCollectionSet;
	bool tarokEnableRecoverRegionTailsAfterSweep; /**< Enable recovering region tail during post sweep of GMP */
	uintptr_t tarokTargetMaxPauseTime; /**< An optional, user specified soft max pause time for PGC's in balanced GC*/
#if defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD)
	bool _isConcurrentCopyForward;
#endif /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */
	enum TarokRegionTailCondidateListSortOrder {
		SORT_ORDER_NOORDER = 0,
		SORT_ORDER_ASCENDING,
		SORT_ORDER_DESCENDING
	};
	TarokRegionTailCondidateListSortOrder tarokTailCandidateListSortOrder;
#endif /* defined (OMR_GC_VLHGC) */

/* OMR_GC_VLHGC (in for all -- see 82589) */
	bool tarokEnableExpensiveAssertions; /**< True if the collector should perform extra consistency verifications which are known to be very expensive or poorly parallelized */
	bool tarokEnableAllocationPointerAssertion;
/* OMR_GC_VLHGC (in for all) */

	MM_SweepPoolManagerAddressOrderedList* sweepPoolManagerAddressOrderedList; /**< Pointer to Sweep Pool Manager for MPAOL, used for LOA and nursery */
	MM_SweepPoolManagerAddressOrderedListBase* sweepPoolManagerSmallObjectArea; /**< Pointer to Sweep Pool Manager for MPSAOL or Hybrid, used for SOA */
	MM_SweepPoolManager* sweepPoolManagerBumpPointer; /**<  Pointer to Sweep Pool Manager for MemoryPoolBumpPointer */

	uint64_t _mainThreadCpuTimeNanos; /**< Total CPU time used by all main threads */

	bool alwaysCallWriteBarrier; /**< was -Xgc:alwayscallwritebarrier specified? */
	bool alwaysCallReadBarrier; /**< was -Xgc:alwaysCallReadBarrier specified? */

	bool _holdRandomThreadBeforeHandlingWorkUnit; /**< Whether we should randomly hold up a thread entering MM_ParallelTask::handleNextWorkUnit() */
	uintptr_t _holdRandomThreadBeforeHandlingWorkUnitPeriod; /**< How often (in terms of number of times MM_ParallelTask::handleNextWorkUnit() is called) to randomly hold up a thread entering MM_ParallelTask::handleNextWorkUnit() */
	bool _forceRandomBackoutsAfterScan; /**< Whether we should force MM_Scavenger::completeScan() to randomly fail due to backout */
	uintptr_t _forceRandomBackoutsAfterScanPeriod; /**< How often (in terms of number of times MM_Scavenger::completeScan() is called) to randomly have MM_Scavenger::completeScan() fail due to backout */

	MM_ReferenceChainWalkerMarkMap* referenceChainWalkerMarkMap; /**< Reference to Reference Chain Walker mark map - will be created at first call and destroyed in Configuration tearDown*/

	bool trackMutatorThreadCategory; /**< Whether we should switch thread categories for mutators doing GC work */

	uintptr_t darkMatterSampleRate;/**< the weight of darkMatterSample for standard gc, default:32, if the weight = 0, disable darkMatterSampling */

	bool pretouchHeapOnExpand; /**< True to pretouch memory during initial heap inflation or heap expansion */

	uintptr_t decommitMinimumFree; /**< percentage of free heap to be retained as committed, default=0 for gencon, complete tenture free memory will be decommitted */

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	bool gcOnIdle; /**< Enables releasing free heap pages if true while systemGarbageCollect invoked with IDLE GC code, default is false */
	bool compactOnIdle; /**< Forces compaction if global GC executed while VM Runtime State set to IDLE, default is false */
#endif /* defined(OMR_GC_IDLE_HEAP_MANAGER) */

#if defined(OMR_VALGRIND_MEMCHECK)
	uintptr_t valgrindMempoolAddr; /**< Memory pool's address for valgrind **/
	J9HashTable *memcheckHashTable; /**< Hash table to store object addresses for valgrind> **/
	MUTEX memcheckHashTableMutex;
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

	bool shouldForceLowMemoryHeapCeilingShiftIfPossible; /**< Whether we should force compressed reference shift to 3 **/
	/* Function Members */
private:

	/**
	 * Validate default page parameters
	 * Search passed pair (page size,page flags) in the arrays provided by Port Library (known by OS)
	 * Note: The only OS supported flags is ZOS however all other platforms have flags array filled by OMRPORT_VMEM_PAGE_FLAG_NOT_USED values
	 * So an example of pair for ZOS: [1M, OMRPORT_VMEM_PAGE_FLAG_PAGEABLE]
	 * An example For AIX: [64K, OMRPORT_VMEM_PAGE_FLAG_NOT_USED]
	 *
	 * @param[in] vm pointer to Java VM object
	 * @param[in] pageSize page size to search
	 * @param[in] pageFlags page flags to search
	 * @param[in] pageSizesArray Page Sizes array (zero terminated)
	 * @param[in] pageFlagsArray Page Flags array (same size as a pageSizesArray array)
	 * @return true if requested pair is discovered
	 */
	static bool validateDefaultPageParameters(uintptr_t pageSize, uintptr_t pageFlags, uintptr_t *pageSizesArray, uintptr_t *pageFlagsArray);

protected:
	virtual bool initialize(MM_EnvironmentBase* env);
	virtual void tearDown(MM_EnvironmentBase* env);
	virtual void computeDefaultMaxHeap(MM_EnvironmentBase* env);
	virtual void reinitializeForRestore(MM_EnvironmentBase* env);
public:
	static MM_GCExtensionsBase* newInstance(MM_EnvironmentBase* env);
	virtual void kill(MM_EnvironmentBase* env);

	/**
	 * Gets a pointer to the base extensions.
	 * @return Pointer to the base extensions.
	 */
	MMINLINE static MM_GCExtensionsBase* getExtensions(OMR_VM* omrVM) { return (MM_GCExtensionsBase*)omrVM->_gcOmrVMExtensions; }

	MMINLINE OMR_VM* getOmrVM() { return _omrVM; }
	MMINLINE void setOmrVM(OMR_VM* omrVM) { _omrVM = omrVM; }

	/**
	 * Gets a pointer to the memory forge
	 * @return Pointer to the memory forge
	 */
	MMINLINE OMR::GC::Forge* getForge() { return &_forge; }

	/**
	 * Return back true if object references are compressed
	 * @return true, if object references are compressed
	 */
	MMINLINE bool compressObjectReferences() {
		return OMR_COMPRESS_OBJECT_REFERENCES(_compressObjectReferences);
	}

	MMINLINE uintptr_t getRememberedCount()
	{
		if (isStandardGC()) {
#if defined(OMR_GC_MODRON_SCAVENGER)
			return static_cast<MM_SublistPool>(rememberedSet).countElements();
#else /* defined(OMR_GC_MODRON_SCAVENGER) */
			return 0;
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
		} else {
			return 0;
		}
	}

	MMINLINE MM_GlobalCollector* getGlobalCollector() { return _globalCollector; }
	MMINLINE void setGlobalCollector(MM_GlobalCollector* collector) { _globalCollector = collector; }

	MMINLINE uintptr_t getLastGlobalGCFreeBytes(){ return lastGlobalGCFreeBytes; }
	MMINLINE void setLastGlobalGCFreeBytes(uintptr_t globalGCFreeBytes){ lastGlobalGCFreeBytes = globalGCFreeBytes;}

#if defined(OMR_GC_OBJECT_MAP)
	MMINLINE MM_ObjectMap *getObjectMap() { return _objectMap; }
	MMINLINE void setObjectMap(MM_ObjectMap *objectMap) { _objectMap = objectMap; }
#endif /* defined(OMR_GC_OBJECT_MAP) */

	MMINLINE bool
	isConcurrentScavengerEnabled()
	{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		return concurrentScavenger;
#else /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
		return false;
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
	}

	MMINLINE bool
	isConcurrentScavengerHWSupported()
	{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		return concurrentScavengerHWSupport;
#else /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
		return false;
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
	}

   MMINLINE bool
   isSoftwareRangeCheckReadBarrierEnabled()
   {
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
      return softwareRangeCheckReadBarrier;
#else /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
      return false;
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
   }

	bool isConcurrentScavengerInProgress();

	MMINLINE bool
	isScavengerEnabled()
	{
#if defined(OMR_GC_MODRON_SCAVENGER)
		return scavengerEnabled;
#else /* defined(OMR_GC_MODRON_SCAVENGER) */
		return false;
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
	}

	MMINLINE bool
	isConcurrentMarkEnabled()
	{
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
		return concurrentMark;
#else /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
		return false;
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
	}

	MMINLINE bool
	isConcurrentSweepEnabled()
	{
#if defined(OMR_GC_CONCURRENT_SWEEP)
		return concurrentSweep;
#else /* defined(OMR_GC_CONCURRENT_SWEEP) */
		return false;
#endif /* defined(OMR_GC_CONCURRENT_SWEEP) */
	}

	MMINLINE bool
	isSegregatedHeap()
	{
#if defined(OMR_GC_COMBINATION_SPEC)
		return _isSegregatedHeap;
#elif defined(OMR_GC_SEGREGATED_HEAP) /* defined(OMR_GC_COMBINATION_SPEC) */
		return true;
#else /* defined(OMR_GC_SEGREGATED_HEAP) */
		return false;
#endif /* defined(OMR_GC_COMBINATION_SPEC) */
	}

	MMINLINE void
	setSegregatedHeap(bool value)
	{
#if defined(OMR_GC_COMBINATION_SPEC)
		_isSegregatedHeap = value;
#endif /* defined(OMR_GC_COMBINATION_SPEC) */
	}

	MMINLINE bool
	isVLHGC()
	{
#if defined(OMR_GC_COMBINATION_SPEC)
		return _isVLHGC;
#elif defined(OMR_GC_VLHGC) /* defined(OMR_GC_COMBINATION_SPEC) */
		return true;
#else /* defined(OMR_GC_VLHGC) */
		return false;
#endif /* defined(OMR_GC_COMBINATION_SPEC) */
	}

	MMINLINE void
	setVLHGC(bool value)
	{
#if defined(OMR_GC_COMBINATION_SPEC)
		_isVLHGC = value;
#endif /* defined(OMR_GC_COMBINATION_SPEC) */
	}

	MMINLINE bool
	isConcurrentCopyForwardEnabled()
	{
#if defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD)
		return _isConcurrentCopyForward;
#else /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */
		return false;
#endif /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */
	}

	MMINLINE bool
	isMetronomeGC()
	{
#if defined(OMR_GC_COMBINATION_SPEC)
		return _isMetronomeGC;
#elif defined(OMR_GC_REALTIME) /* defined(OMR_GC_COMBINATION_SPEC) */
		return true;
#else /* defined(OMR_GC_REALTIME) */
		return false;
#endif /* defined(OMR_GC_COMBINATION_SPEC) */
	}

	MMINLINE void
	setMetronomeGC(bool value)
	{
#if defined(OMR_GC_COMBINATION_SPEC)
		_isMetronomeGC = value;
#endif /* defined(OMR_GC_COMBINATION_SPEC) */
	}

	MMINLINE bool
	isStandardGC()
	{
#if defined(OMR_GC_COMBINATION_SPEC)
		return _isStandardGC;
#elif defined(OMR_GC_MODRON_STANDARD) /* defined(OMR_GC_COMBINATION_SPEC) */
		return true;
#else /* defined(OMR_GC_MODRON_STANDARD) */
		return false;
#endif /* defined(OMR_GC_COMBINATION_SPEC) */
	}

	MMINLINE void
	setStandardGC(bool value)
	{
#if defined(OMR_GC_COMBINATION_SPEC)
		_isStandardGC = value;
#endif /* defined(OMR_GC_COMBINATION_SPEC) */
	}

	MMINLINE J9HookInterface** getPrivateHookInterface() { return J9_HOOK_INTERFACE(privateHookInterface); }
	MMINLINE J9HookInterface** getOmrHookInterface() { return J9_HOOK_INTERFACE(omrHookInterface); }

	MMINLINE uintptr_t getMinimumFreeEntrySize() { return tlhMinimumSize; }

	MMINLINE MM_Heap* getHeap() { return heap; }

	MMINLINE void
	setGuaranteedNurseryRange(void* start, void* end)
	{
#if defined(OMR_GC_MODRON_SCAVENGER)
		_guaranteedNurseryStart = start;
		_guaranteedNurseryEnd = end;
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
	}

	MMINLINE void
	getGuaranteedNurseryRange(void** start, void** end)
	{
#if defined(OMR_GC_MODRON_SCAVENGER)
		*start = _guaranteedNurseryStart;
		*end = _guaranteedNurseryEnd;
#else /* defined(OMR_GC_MODRON_SCAVENGER) */
		*start = NULL;
		*end = NULL;
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
	}

	MMINLINE bool
	isScavengerRememberedSetInOverflowState()
	{
#if defined(OMR_GC_MODRON_SCAVENGER)
		return _isRememberedSetInOverflow;
#else /* defined(OMR_GC_MODRON_SCAVENGER) */
		return false;
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
	}

	MMINLINE void
	setScavengerRememberedSetOverflowState()
	{
#if defined(OMR_GC_MODRON_SCAVENGER)
		_isRememberedSetInOverflow = true;
#endif /* OMR_GC_MODRON_SCAVENGER */
	}

	MMINLINE void
	clearScavengerRememberedSetOverflowState()
	{
#if defined(OMR_GC_MODRON_SCAVENGER)
		_isRememberedSetInOverflow = false;
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
	}

	MMINLINE bool
	isScavengerBackOutFlagRaised()
	{
#if defined(OMR_GC_MODRON_SCAVENGER)
		return backOutFlagCleared < _backOutState;
#else /* defined(OMR_GC_MODRON_SCAVENGER) */
		return false;
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
	}

	MMINLINE void
	setConcurrentGlobalGCInProgress(bool inProgress)
	{
#if defined(OMR_GC_MODRON_SCAVENGER)
		_concurrentGlobalGCInProgress = inProgress;
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
	}

#if defined(OMR_GC_MODRON_SCAVENGER)
	MMINLINE void
	setScavengerBackOutState(BackOutState backOutState)
	{
		_backOutState = backOutState;
	}

	MMINLINE BackOutState
	getScavengerBackOutState()
	{
		return _backOutState;
	}

	MMINLINE bool
	shouldScavengeNotifyGlobalGCOfOldToOldReference()
	{
		return _concurrentGlobalGCInProgress;
	}

	/**
	 * Determine whether Adaptive Threading is enabled. AdaptiveGCThreading flag
	 * is not sufficient; Adaptive threading must be ignored if GC thread count is forced.
	 * @return TRUE if adaptive threading routines can proceed, FALSE otherwise
	 */
	MMINLINE bool
	adaptiveThreadingEnabled()
	{
		return (adaptiveGCThreading && !gcThreadCountForced);
	}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	/**
	 * Returns TRUE if an object is old, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object is in the old area, FALSE otherwise
	 */
	MMINLINE bool
	isOld(omrobjectptr_t objectPtr)
	{
		return ((uintptr_t)objectPtr - (uintptr_t)_tenureBase) < _tenureSize;
	}

	/**
	 * Returns TRUE if a contiguous range of heap slots is contained in tenure space, FALSE otherwise.
	 * @param baseSlotPtr Pointer to first slot in range
	 * @param topSlotPtr Pointer to first slot >= baseSlotPtr and NOT in range
	 * @return TRUE if entire range is in the old area, FALSE otherwise
	 */
	MMINLINE bool
	isOld(void *baseSlotPtr, void *topSlotPtr)
	{
		return ((uintptr_t)baseSlotPtr >= (uintptr_t)_tenureBase) && ((uintptr_t)topSlotPtr - (uintptr_t)_tenureBase) < _tenureSize;
	}

	MMINLINE bool
	isFvtestForce(uintptr_t *forceMax, uintptr_t *forceCntr)
	{
		bool result = false;
		if (0 != *forceMax) {
			if (0 == *forceCntr) {
				result = true;
				*forceCntr = *forceMax;
			}
			*forceCntr -= 1;
		}
		return result;
	}

	MMINLINE bool
	isFvtestForceSweepChunkArrayCommitFailure()
	{
		return isFvtestForce(&fvtest_forceSweepChunkArrayCommitFailure, &fvtest_forceSweepChunkArrayCommitFailureCounter);
	}

	MMINLINE bool
	isFvtestForceMarkMapCommitFailure()
	{
		return isFvtestForce(&fvtest_forceMarkMapCommitFailure, &fvtest_forceMarkMapCommitFailureCounter);
	}

	MMINLINE bool
	isFvtestForceMarkMapDecommitFailure()
	{
		return isFvtestForce(&fvtest_forceMarkMapDecommitFailure, &fvtest_forceMarkMapDecommitFailureCounter);
	}

	MMINLINE bool
	isFvtestForceReferenceChainWalkerMarkMapCommitFailure()
	{
		return isFvtestForce(&fvtest_forceReferenceChainWalkerMarkMapCommitFailure, &fvtest_forceReferenceChainWalkerMarkMapCommitFailureCounter);
	}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	MMINLINE bool
	isFvtestForceConcurrentTLHMarkMapCommitFailure()
	{
		return isFvtestForce(&fvtest_forceConcurrentTLHMarkMapCommitFailure, &fvtest_forceConcurrentTLHMarkMapCommitFailureCounter);
	}

	MMINLINE bool
	isFvtestForceConcurrentTLHMarkMapDecommitFailure()
	{
		return isFvtestForce(&fvtest_forceConcurrentTLHMarkMapDecommitFailure, &fvtest_forceConcurrentTLHMarkMapDecommitFailureCounter);
	}
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

	MMINLINE bool
	isFvtestForceCardTableCommitFailure()
	{
		return isFvtestForce(&fvtest_forceCardTableCommitFailure, &fvtest_forceCardTableCommitFailureCounter);
	}

	MMINLINE bool
	isFvtestForceCardTableDecommitFailure()
	{
		return isFvtestForce(&fvtest_forceCardTableDecommitFailure, &fvtest_forceCardTableDecommitFailureCounter);
	}

	/**
	 * Get run-time object alignment in bytes
	 * @return object alignment in bytes
	 */
	MMINLINE uintptr_t
	getObjectAlignmentInBytes()
	{
		return objectModel.getObjectAlignmentInBytes();
	}

	MMINLINE float
	getDarkMatterCompactThreshold() {
		return darkMatterCompactThreshold;
	}

	/**
	 * Set Tenure address range
	 * @param base low address of Old subspace range
	 * @param size size of Old subspace in bytes
	 */
	virtual void setTenureAddressRange(void* base, uintptr_t size)
	{
		_tenureBase = base;
		_tenureSize = size;

		/* todo: dagar move back to MemorySubSpaceGeneric addTenureRange() and removeTenureRange() once
		 * heapBaseForBarrierRange0 heapSizeForBarrierRange0 can be removed from J9VMThread
		 *
		 * setTenureAddressRange() can be removed from GCExtensions.hpp and made inline again
		 */
		GC_OMRVMThreadListIterator vmThreadListIterator(_omrVM);
		while (OMR_VMThread* walkThread = vmThreadListIterator.nextOMRVMThread()) {
			walkThread->lowTenureAddress = heapBaseForBarrierRange0;
			walkThread->highTenureAddress = (void*)((uintptr_t)heapBaseForBarrierRange0 + heapSizeForBarrierRange0);
			walkThread->heapBaseForBarrierRange0 = heapBaseForBarrierRange0;
			walkThread->heapSizeForBarrierRange0 = heapSizeForBarrierRange0;
		}
	}

	virtual void identityHashDataAddRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress);
	virtual void identityHashDataRemoveRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress);

#define CONCURRENT_SCAVENGER_PAGE_MINIMUM_SECTION_SIZE (512 * 1024)
#define CONCURRENT_SCAVENGER_PAGE_SECTIONS 64

	/**
	 * Calculate Concurrent Scavenger Page size based on projected maximum Nursery size
	 *
	 * @param nurserySize projected maximum Nursery size
	 */
	MMINLINE void
	calculateConcurrentScavengerPageParameters(uintptr_t nurserySize)
	{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		/*
		 * Concurrent Scavenger Page is a virtual memory page size 32M or larger aligned to it's size
		 * This page is split to 64 equal Sections
		 * We need to select Concurrent Scavenger Page size to cover entire fully expanded Nursery
		 * To do so, find closest power of 2 larger or equal then maximum Nursery size and store 1/64th of it as a Section size
		 * Section size can not be smaller then 32M / 64 = 512K
		 */
		uintptr_t log2 = MM_Math::floorLog2(nurserySize);
		if (nurserySize > ((uintptr_t)1 << log2)) {
			log2 += 1;
		}
		concurrentScavengerPageSectionSize = ((uintptr_t)1 << log2) / CONCURRENT_SCAVENGER_PAGE_SECTIONS;
		if (concurrentScavengerPageSectionSize < CONCURRENT_SCAVENGER_PAGE_MINIMUM_SECTION_SIZE) {
			concurrentScavengerPageSectionSize = CONCURRENT_SCAVENGER_PAGE_MINIMUM_SECTION_SIZE;
		}
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
	}

	/**
	 * Get value of debugConcurrentScavengerPageAlignment if OMR_GC_CONCURRENT_SCAVENGER enabled
	 * or false in general case
	 *
	 * @return value of debugConcurrentScavengerPageAlignment or false
	 */
	MMINLINE bool
	isDebugConcurrentScavengerPageAlignment()
	{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		return debugConcurrentScavengerPageAlignment;
#else /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
		return false;
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
	}

	/**
	 * Set value of debugConcurrentScavengerPageAlignment if OMR_GC_CONCURRENT_SCAVENGER enabled
	 *
	 * @param debug value to set to debugConcurrentScavengerPageAlignment
	 */
	MMINLINE void
	setDebugConcurrentScavengerPageAlignment(bool debug)
	{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		debugConcurrentScavengerPageAlignment = debug;
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
	}

	/**
	 * Get value of concurrentScavengerPageSectionSize if OMR_GC_CONCURRENT_SCAVENGER enabled
	 *
	 * @return  value of concurrentScavengerPageSectionSize
	 */
	MMINLINE uintptr_t
	getConcurrentScavengerPageSectionSize()
	{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		return concurrentScavengerPageSectionSize;
#else /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
		return 0;
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
	}

	/**
	 * Get value of concurrentScavengerPageStartAddress if OMR_GC_CONCURRENT_SCAVENGER enabled
	 *
	 * @return value of concurrentScavengerPageStartAddress
	 */
	MMINLINE void *
	getConcurrentScavengerPageStartAddress()
	{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		return concurrentScavengerPageStartAddress;
#else /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
		return (void *)UDATA_MAX;
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
	}

	/**
	 * Set value of concurrentScavengerPageStartAddress if OMR_GC_CONCURRENT_SCAVENGER enabled
	 *
	 * @param address value of concurrentScavengerPageStartAddress
	 */
	MMINLINE void
	setConcurrentScavengerPageStartAddress(void *address)
	{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		concurrentScavengerPageStartAddress = address;
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
	}

	/**
	 * Check if we need to disable inline allocation
	 */
	MMINLINE bool
	needDisableInlineAllocation() {
		return (fvtest_disableInlineAllocation || instrumentableAllocateHookEnabled || disableInlineCacheForAllocationThreshold);
	}

	bool isSATBBarrierActive();
	bool usingSATBBarrier();

#if defined(J9VM_OPT_CRIU_SUPPORT)
	/**
	 * Helper function to determine whether a GC reinitialization is taking place
	 * as a result of a VM snapshot restore.
	 * @return boolean indicating whether GC reinitialization is taking place.
	 */
	MMINLINE virtual bool reinitializationInProgress() { return false; }
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

	/**
	 * Check if the address is within the heap.
	 *
	 * @param address   pointer to arraylet data that we will check to see if it resides in the heap
	 * @return true if the address given is within the heap
	 */
	bool isAddressWithinHeap(void *address);

	MM_GCExtensionsBase()
		: MM_BaseVirtual()
#if defined(OMR_GC_MODRON_SCAVENGER)
		, _guaranteedNurseryStart(NULL)
		, _guaranteedNurseryEnd(NULL)
		, _isRememberedSetInOverflow(false)
		, _backOutState(backOutFlagCleared)
		, _concurrentGlobalGCInProgress(false)
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		, debugConcurrentScavengerPageAlignment(false)
		, concurrentScavengerPageSectionSize(0)
		, concurrentScavengerPageStartAddress((void *)UDATA_MAX)
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
		, _compressObjectReferences(false)
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
		, _omrVM(NULL)
		,_forge()
		, _globalCollector(NULL)
		, lastGlobalGCFreeBytes(0)
#if defined(OMR_GC_OBJECT_MAP)
		, _objectMap(NULL)
#endif /* defined(OMR_GC_OBJECT_MAP) */
		, _lazyCollectorInit(false)
		, collectorLanguageInterface(NULL)
		, _tenureBase(NULL)
		, _tenureSize(0)
#if defined(OMR_GC_MODRON_SCAVENGER)
		, scavenger(NULL)
		, _mainThreadTenureTLHRemainderBase(NULL)
		, _mainThreadTenureTLHRemainderTop(NULL)
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
		, environments(NULL)
		, excessiveGCStats()
#if defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME)
		, globalGCStats()
#endif /* OMR_GC_MODRON_STANDARD || OMR_GC_REALTIME */
#if defined(OMR_GC_MODRON_SCAVENGER)
		, incrementScavengerStats()
		, scavengerStats()
		, copyScanRatio()
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#if defined(OMR_GC_VLHGC)
		, globalVLHGCStats()
#endif /* defined(OMR_GC_VLHGC) */
#if defined(OMR_GC_CONCURRENT_SWEEP)
		, concurrentSweep(false)
#endif /* defined(OMR_GC_CONCURRENT_SWEEP) */
		, largePageWarnOnError(false)
		, largePageFailOnError(false)
		, largePageFailedToSatisfy(false)
#if defined(OMR_GC_DOUBLE_MAP_ARRAYLETS)
		, isArrayletDoubleMapRequested(false)
		, isArrayletDoubleMapAvailable(false)
#endif /* defined(OMR_GC_DOUBLE_MAP_ARRAYLETS) */
#if defined(OMR_GC_SPARSE_HEAP_ALLOCATION)
		, isVirtualLargeObjectHeapRequested(false)
		, isVirtualLargeObjectHeapEnabled(false)
#endif /* defined(OMR_GC_SPARSE_HEAP_ALLOCATION) */
		, requestedPageSize(0)
		, requestedPageFlags(OMRPORT_VMEM_PAGE_FLAG_NOT_USED)
		, gcmetadataPageSize(0)
		, gcmetadataPageFlags(OMRPORT_VMEM_PAGE_FLAG_NOT_USED)
		, sparseHeapPageSize(0)
		, sparseHeapPageFlags(OMRPORT_VMEM_PAGE_FLAG_NOT_USED)
#if defined(OMR_GC_MODRON_SCAVENGER)
		, rememberedSet()
		, oldHeapSizeOnLastGlobalGC(UDATA_MAX)
		, freeOldHeapSizeOnLastGlobalGC(UDATA_MAX)
		, concurrentKickoffTenuringHeadroom((float)0.02)
		, tenureBytesDeviationBoost((float)2)
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#if defined(OMR_GC_REALTIME)
		, sATBBarrierRememberedSet(NULL)
#endif /* defined(OMR_GC_REALTIME) */
		, heapBaseForBarrierRange0(NULL)
		, heapSizeForBarrierRange0(0)
#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
		, shadowHeapBase(0)
		, shadowHeapTop(0)
		, shadowHeapHandle()
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */
		, doOutOfLineAllocationTrace(true) /* Tracing after ever x bytes allocated per thread. Enabled by default. */
		, doFrequentObjectAllocationSampling(false) /* Finds most frequently allocated classes. Disabled by default. */
		, oolObjectSamplingBytesGranularity(16*1024*1024) /* Default granularity set to 16M (shows <1% perf loss). */
		, objectSamplingBytesGranularity(UDATA_MAX) /* default UDATA_MAX (disabled) */
		, frequentObjectAllocationSamplingRate(100)
		, frequentObjectsStats(NULL)
		, frequentObjectAllocationSamplingDepth(0)
		, estimateFragmentation(GLOBALGC_ESTIMATE_FRAGMENTATION)
		, processLargeAllocateStats(true) /* turn on processLargeAllocateStats by default */
		, largeObjectAllocationProfilingThreshold(512)
		, largeObjectAllocationProfilingVeryLargeObjectThreshold(UDATA_MAX)
		, largeObjectAllocationProfilingVeryLargeObjectSizeClass(0)
		, largeObjectAllocationProfilingSizeClassRatio(120)
		, largeObjectAllocationProfilingTopK(8)
		, freeMemoryProfileMaxSizeClasses(0)
		, gcExclusiveAccessThreadId(NULL)
		, gcExclusiveAccessMutex(NULL)
		, _lightweightNonReentrantLockPool(NULL)
#if defined(OMR_GC_COMBINATION_SPEC)
		, _isSegregatedHeap(false)
		, _isVLHGC(false)
		, _isMetronomeGC(false)
		, _isStandardGC(false)
#endif /* defined(OMR_GC_COMBINATION_SPEC) */
		, tlhMinimumSize(MINIMUM_TLH_SIZE)
		, tlhMaximumSize(131072)
		, tlhInitialSize(2048)
		, tlhIncrementSize(4096)
		, tlhSurvivorDiscardThreshold(tlhMinimumSize)
		, tlhTenureDiscardThreshold(tlhMinimumSize)
		, allocationStats()
		, bytesAllocatedMost(0)
		, vmThreadAllocatedMost(NULL)
		, gcModeString(NULL)
		, splitFreeListSplitAmount(0)
		, splitFreeListAmountForced(false)
		, splitFreeListNumberChunksPrepared(0)
		, enableHybridMemoryPool(false)
		, largeObjectArea(false)
#if defined(OMR_GC_LARGE_OBJECT_AREA)
		, largeObjectMinimumSize(64 * 1024)
		, largeObjectAreaInitialRatio(0.050) /* initial LOA 5% */
		, largeObjectAreaMinimumRatio(0.01) /* initial LOA 1% */
		, largeObjectAreaMaximumRatio(0.500) /* maximum LOA 50% */
		, debugLOAFreelist(false)
		, debugLOAAllocate(false)
		, loaFreeHistorySize(15)
		, lastGlobalGCFreeBytesLOA(0)
		, concurrentMetering(METER_BY_SOA)
		, minimumContractionRatio(DEFAULT_MINIMUM_CONTRACTION_RATIO)
#endif /* defined(OMR_GC_LARGE_OBJECT_AREA) */
		, disableExplicitGC(false)
		, heapAlignment(HEAP_ALIGNMENT)
		, absoluteMinimumOldSubSpaceSize(MINIMUM_OLD_SPACE_SIZE)
		, absoluteMinimumNewSubSpaceSize(MINIMUM_NEW_SPACE_SIZE)
		, darkMatterCompactThreshold((float)0.15)
		, pageFragmentationCompactThreshold((float)0.10)
		, parSweepChunkSize(0)
		, heapExpansionMinimumSize(1024 * 1024)
		, heapExpansionMaximumSize(0)
		, heapFreeMinimumRatioDivisor(100)
		, heapFreeMinimumRatioMultiplier(30)
		, heapFreeMaximumRatioDivisor(100)
		, heapFreeMaximumRatioMultiplier(60)
		, heapExpansionGCRatioThreshold()
		, heapContractionGCRatioThreshold()
		, heapExpansionStabilizationCount(0)
		, heapContractionStabilizationCount(3)
		, heapSizeStartupHintConservativeFactor((float)0.7)
		, heapSizeStartupHintWeightNewValue((float)0.8)
		, useGCStartupHints(true)
		, workpacketCount(0) /* only set if -Xgcworkpackets specified */
		, packetListSplit(0)
		, packetListSplitForced(false)
		, markingArraySplitMaximumAmount(DEFAULT_ARRAY_SPLIT_MAXIMUM_SIZE)
		, markingArraySplitMinimumAmount(DEFAULT_ARRAY_SPLIT_MINIMUM_SIZE)
		, rootScannerStatsEnabled(false)
		, rootScannerStatsUsed(false)
		, fvtest_forceOldResize(0)
		, fvtest_oldResizeCounter(0)
#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)
		, fvtest_scanCacheCount(0)
#if defined(OMR_GC_MODRON_SCAVENGER)
		, fvtest_forceScavengerBackout(0)
		, fvtest_backoutCounter(0)
		, fvtest_forcePoisonEvacuate(0)
		, fvtest_forceNurseryResize(0)
		, fvtest_nurseryResizeCounter(0)
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */
		, fvtest_alwaysApplyOverflowRounding(0)
		, fvtest_forceExcessiveAllocFailureAfter(0)
		, fvtest_verifyHeapAbove(0)
		, fvtest_verifyHeapBelow(0)
#if defined(OMR_GC_VLHGC)
		, fvtest_tarokVerifyMarkMapClosure(0)
#endif /* defined(OMR_GC_VLHGC) */
		, fvtest_disableInlineAllocation(0)
		, fvtest_forceSweepChunkArrayCommitFailure(0)
		, fvtest_forceSweepChunkArrayCommitFailureCounter(0)
#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
		, fvtest_enableReadBarrierVerification(0)
		, fvtest_enableMonitorObjectsReadBarrierVerification(0)
		, fvtest_enableClassStaticsReadBarrierVerification(0)
		, fvtest_enableJNIGlobalWeakReadBarrierVerification(0)
		, fvtest_enableHeapReadBarrierVerification(0)
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */
		, fvtest_forceMarkMapCommitFailure(0)
		, fvtest_forceMarkMapCommitFailureCounter(0)
		, fvtest_forceMarkMapDecommitFailure(0)
		, fvtest_forceMarkMapDecommitFailureCounter(0)
		, fvtest_forceReferenceChainWalkerMarkMapCommitFailure(0)
		, fvtest_forceReferenceChainWalkerMarkMapCommitFailureCounter(0)
		, fvtest_forceCopyForwardHybridRatio(0)
		, softMx(0) /* softMx only set if specified */
#if defined(OMR_GC_BATCH_CLEAR_TLH)
		, batchClearTLH(0)
#endif /* defined(OMR_GC_BATCH_CLEAR_TLH) */
		, gcThreadCount(0)
		, gcThreadCountSpecified(false)
		, gcThreadCountForced(false)
		, dispatcherHybridNotifyThreadBound(16)
#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)
		, scavengerScanOrdering(OMR_GC_SCAVENGER_SCANORDERING_NONE)
		/* Start of options relating to dynamicBreadthFirstScanOrdering */
		, gcCountBetweenHotFieldSort(1)
		, gcCountBetweenHotFieldSortMax(6)
		, adaptiveGcCountBetweenHotFieldSort(false)
		, depthCopyTwoPaths(true)
		, depthCopyThreePaths(false)
		, alwaysDepthCopyFirstOffset(false)
		, allowPermanantHotFields (false)
		, hotFieldResettingEnabled (false)
		, maxConsecutiveHotFieldSelections(10)
		, gcCountBetweenHotFieldReset(100)
		, depthCopyMax(3)
		, maxHotFieldListLength(10)
		, minCpuUtil (1)
		/* End of options relating to dynamicBreadthFirstScanOrdering */
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */
#if defined(OMR_GC_MODRON_SCAVENGER)
		, scvTenureRatioHigh(OMR_SCV_TENURE_RATIO_HIGH)
		, scvTenureRatioLow(OMR_SCV_TENURE_RATIO_LOW)
		, scvTenureFixedTenureAge(OBJECT_HEADER_AGE_MAX)
		, scvTenureAdaptiveTenureAge(0)
		, scvTenureStrategySurvivalThreshold(0.99)
		, scvTenureStrategyFixed(false)
		, scvTenureStrategyAdaptive(true)
		, scvTenureStrategyLookback(true)
		, scvTenureStrategyHistory(true)
		, scavengerEnabled(false)
		, scavengerRsoScanUnsafe(false)
		, cacheListSplit(0)
		, cacheListSplitForced(false)
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		, softwareRangeCheckReadBarrier(false)
		, softwareRangeCheckReadBarrierForced(false)
		, concurrentScavenger(false)
		, concurrentScavengerForced(false)
		, concurrentScavengerHWSupport(false)
		, concurrentScavengerBackgroundThreads(1)
		, concurrentScavengerBackgroundThreadsForced(false)
		, concurrentScavengerSlack(0)
		, concurrentScavengerAllocDeviationBoost(2.0)
		, concurrentScavengeExhaustiveTermination(true)
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
		, scavengerFailedTenureThreshold(0)
		, maxScavengeBeforeGlobal(0)
		, scvArraySplitMaximumAmount(DEFAULT_ARRAY_SPLIT_MAXIMUM_SIZE)
		, scvArraySplitMinimumAmount(DEFAULT_ARRAY_SPLIT_MINIMUM_SIZE)
		, scavengerScanCacheMaximumSize(DEFAULT_SCAN_CACHE_MAXIMUM_SIZE)
		, scavengerScanCacheMinimumSize(DEFAULT_SCAN_CACHE_MINIMUM_SIZE)
		, tiltedScavenge(true)
		, debugTiltedScavenge(false)
		, survivorSpaceMinimumSizeRatio(0.10)
		, survivorSpaceMaximumSizeRatio(0.50)
		, tiltedScavengeMaximumIncrease(0.10)
		, scavengerCollectorExpandRatio(0.1)
		, scavengerMaximumCollectorExpandSize(1024 * 1024)
		, dynamicNewSpaceSizing(true)
		, debugDynamicNewSpaceSizing(false)
		, dnssAvoidMovingObjects(true)
		, dnssExpectedRatioMinimum()
		, dnssExpectedRatioMaximum()
		, dnssWeightedTimeRatioFactorIncreaseSmall(0.2)
		, dnssWeightedTimeRatioFactorIncreaseMedium(0.35)
		, dnssWeightedTimeRatioFactorIncreaseLarge(0.5)
		, dnssWeightedTimeRatioFactorDecrease(0.05)
		, dnssMaximumExpansion(1.0)
		, dnssMaximumContraction(0.5)
		, dnssMinimumExpansion(0.0)
		, dnssMinimumContraction(0.0)
		, enableSplitHeap(false)
		, aliasInhibitingThresholdPercentage(0.20)
		, adaptiveGCThreading(true)
		, adaptiveThreadingSensitivityFactor(1.0f)
		, adaptiveThreadingWeightActiveThreads(0.50f)
		, adaptiveThreadBooster(0.85f)
		, splitHeapSection(HEAP_INITIALIZATION_SPLIT_HEAP_UNKNOWN)
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
		, globalMaximumContraction(0.05) /* by default, contract must be at most 5% of the committed heap */
		, globalMinimumContraction(0.01) /* by default, contract must be at least 1% of the committed heap */
		, excessiveGCEnabled()
		, isRecursiveGC(false)
		, didGlobalGC(false)
		, excessiveGCLevel(excessive_gc_normal)
		, excessiveGCnewRatioWeight((float)0.95)
		, excessiveGCratio(95)
		, excessiveGCFreeSizeRatio((float)0.03)
		, heap(NULL)
		, largeObjectVirtualMemory(NULL)
		, heapRegionManager(NULL)
		, memoryManager(NULL)
		, aggressive(0)
		, sweepHeapSectioning(NULL)
#if defined(OMR_GC_MODRON_COMPACTION)
		, compactOnGlobalGC(0) /* By default we will only compact on triggers, no forced compactions */
		, noCompactOnGlobalGC(0)
		, compactOnSystemGC(0)
		, nocompactOnSystemGC(0)
		, compactToSatisfyAllocate(false)
#endif /* defined(OMR_GC_MODRON_COMPACTION) */
		, payAllocationTax(false)
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
		, concurrentMark(false)
		, concurrentKickoffEnabled(true)
		, concurrentSlackFragmentationAdjustmentWeight(0.0)
		, debugConcurrentMark(false)
		, optimizeConcurrentWB(true)
		, concurrentLevel(8)
		, concurrentBackground(1)
		, concurrentSlack(0)
		, cardCleanPass2Boost(2)
		, cardCleaningPasses(2)
		, fvtest_concurrentCardTablePreparationDelay(0)
		, fvtest_forceConcurrentTLHMarkMapCommitFailure(0)
		, fvtest_forceConcurrentTLHMarkMapCommitFailureCounter(0)
		, fvtest_forceConcurrentTLHMarkMapDecommitFailure(0)
		, fvtest_forceConcurrentTLHMarkMapDecommitFailureCounter(0)
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
		, fvtest_forceCardTableCommitFailure(0)
		, fvtest_forceCardTableCommitFailureCounter(0)
		, fvtest_forceCardTableDecommitFailure(0)
		, fvtest_forceCardTableDecommitFailureCounter(0)
		, dispatcher(NULL)
#if defined(J9VM_OPT_CRIU_SUPPORT)
		, checkpointGCthreadCount(4)
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
		, cardTable(NULL)
		, memoryMax(0)
		, initialMemorySize(0)
		, minNewSpaceSize(0)
		, newSpaceSize(0)
		, maxNewSpaceSize(0)
		, minOldSpaceSize(0)
		, oldSpaceSize(0)
		, maxOldSpaceSize(0)
		, allocationIncrement(0)
		, fixedAllocationIncrement(0)
		, lowMinimum(0)
		, allowMergedSpaces(1)
		, maxSizeDefaultMemorySpace(0)
		, allocationIncrementSetByUser(0)
		, overflowSafeAllocSize(0)
		, usablePhysicalMemory(0)
#if defined(OMR_GC_REALTIME)
		, RTC_Frequency(2048) // must be power of 2 - translates to ~488us delay
		, itPeriodMicro(1000)
		, hrtPeriodMicro(METRONOME_DEFAULT_HRT_PERIOD_MICRO)
		, debugWriteBarrier(0)
		, timeWindowMicro(METRONOME_DEFAULT_TIME_WINDOW_MICRO)
		, beatMicro(METRONOME_DEFAULT_BEAT_MICRO)
		, overrideHiresTimerCheck(false)
		, targetUtilizationPercentage(70)
		, gcTrigger(0)                  // bytes
		, gcInitialTrigger(0)           // bytes
		, headRoom(1024 * 1024)          // bytes
		, synchronousGCOnOOM(true)
		, extraYield(false)
		, realtimeGC(NULL)
		, fixHeapForWalk(false)
		, minArraySizeToSetAsScanned(0)
		, overflowCacheCount(0) /**< initial value of 0.  This is set in workpackets initialization or via the commandline */
		, concurrentSweepingEnabled(false)
		, concurrentTracingEnabled(false)
#endif /* defined(OMR_GC_REALTIME) */
		, instrumentableAllocateHookEnabled(false) /* by default the hook J9HOOK_VM_OBJECT_ALLOCATE_INSTRUMENTABLE is disabled */
		, previousMarkMap(NULL)
		, globalAllocationManager(NULL)
#if defined(OMR_GC_REALTIME) || defined(OMR_GC_SEGREGATED_HEAP)
		, managedAllocationContextCount(0)
#endif /* defined(OMR_GC_REALTIME) || defined(OMR_GC_SEGREGATED_HEAP) */
#if defined(OMR_GC_SEGREGATED_HEAP)
		, defaultSizeClasses(NULL)
#endif /* defined(OMR_GC_SEGREGATED_HEAP) */
#if defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD)
		, heapRegionStateTable(NULL)
#endif /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */
		, distanceToYieldTimeCheck(0)
		, traceCostToCheckYield(500) /* weighted sum of marked objects and scanned pointers before we check yield in main tracing loop */
		, sweepCostToCheckYield(500) /* weighted count of free chunks/marked objects before we check yield in sweep small loop */
		, splitAvailableListSplitAmount(0)
		, newThreadAllocationColor(0)
		, minimumFreeEntrySize((uintptr_t)-1) /* -1 => user did not override default minimumFreeEntrySize */
		, arrayletsPerRegion(0)
		, verbose(0)
		, debug(0)
		, allocationTrackerMaxTotalError(UDATA_MAX)
		, allocationTrackerMaxThreshold(128 * 1024) /* 128 KB */
		, allocationTrackerFlushThreshold(allocationTrackerMaxThreshold)
		, allocationCacheMinimumSize(0)
		, allocationCacheMaximumSize(16384)
		, allocationCacheInitialSize(256)
		, allocationCacheIncrementSize(256)
		, nonDeterministicSweep(false)
		, configuration(NULL)
		, verboseGCManager(NULL)
		, verbosegcCycleTime(1000)  /* by default metronome outputs verbosegc every 1sec */
		, verboseExtensions(false)
		, verboseNewFormat(true)
		, bufferedLogging(false)
		, lowAllocationThreshold(UDATA_MAX)
		, highAllocationThreshold(UDATA_MAX)
		, disableInlineCacheForAllocationThreshold(false)
		, disableInlineAllocationForSamplingBytesGranularity(false)
		, heapCeiling(0) /* default for normal platforms is 0 (i.e. no ceiling) */
		, heapInitializationFailureReason(HEAP_INITIALIZATION_FAILURE_REASON_NO_ERROR)
		, scavengerAlignHotFields(true) /* VM Design 1774: hot field alignment is on by default */
		, suballocatorInitialSize(SUBALLOCATOR_INITIAL_SIZE) /* default for J9Heap suballocator initial size is 200 MB */
		, suballocatorCommitSize(SUBALLOCATOR_COMMIT_SIZE) /* default for J9Heap suballocator commit size is 50 MB */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		, shouldAllowShiftingCompression(true) /* VM Design 1810: shifting compression enabled, by default, for compressed refs */
		, shouldForceSpecifiedShiftingCompression(0)
		, forcedShiftingCompressionAmount(0)
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		, preferredHeapBase(PREFERRED_HEAP_BASE)
		, currentEnvironmentCount(0)
		, regionSize(0)
		, _numaManager()
		, numaForced(false)
		, padToPageSize(false)
		, fvtest_disableExplictMainThread(false)
#if defined(OMR_GC_VLHGC)
		, tarokDebugEnabled(false) /* by default, we turn off the Tarok debug options since they are an appreciable performance hit */
		, tarokGlobalMarkIncrementTimeMillis(0)
		, fvtest_tarokForceNUMANode(UDATA_MAX)
		, fvtest_tarokFirstContext(0)
		, tarokEnableScoreBasedAtomicCompact(true)
		, tarokIdealEdenMinimumBytes(0)
		, tarokIdealEdenMaximumBytes(0)
		, tarokEnableIncrementalGMP(true)
		, tarokNurseryMaxAge()
		, tarokRememberedSetCardListMaxSize(0)
		, tarokRememberedSetCardListSize(0)
		, tarokPGCtoGMPNumerator(1)
		, tarokPGCtoGMPDenominator(1)
		, tarokGMPIntermission(UDATA_MAX)
		, tarokAutomaticGMPIntermission(true)
		, tarokRegionMaxAge(0)
		, tarokKickoffHeadroomInBytes(0)
		, tarokForceKickoffHeadroomInBytes(false)
		, tarokKickoffHeadroomRegionRate(2)
		, rememberedSetCardBucketPool(NULL)
		, tarokEnableDynamicCollectionSetSelection(true)
		, tarokDynamicCollectionSetSelectionAbsoluteBudget(0)
		, tarokDynamicCollectionSetSelectionPercentageBudget(0.07)
		, tarokCoreSamplingAbsoluteBudget(0)
		, tarokCoreSamplingPercentageBudget(0.03)
		, tarokTgcSetSelectionDataTable(NULL)
		, tarokTgcEnableRememberedSetDuplicateDetection(false)
		, tarokPGCShouldCopyForward(true)
		, tarokPGCShouldMarkCompact(false)
		, interRegionRememberedSet(NULL)
		, tarokEnableStableRegionDetection(true)
		, tarokDefragmentEmptinessThreshold(0.0)
		, tarokAttachedThreadsAreCommon(true)
		, tarokCopyForwardFragmentationTarget(0.05)
		, tarokEnableCardScrubbing(true)
		, tarokEnableConcurrentGMP(true)
		, compactGroupPersistentStats(NULL)
		, classLoaderRememberedSet(NULL)
		, tarokEnableIncrementalClassGC(true)
		, tarokEnableCompressedCardTable(true)
		, compressedCardTable(NULL)
		, tarokEnableLeafFirstCopying(false)
		, tarokMaximumAgeInBytes(0)
		, tarokMaximumNurseryAgeInBytes(0)
		, tarokAllocationAgeEnabled(false)
		, tarokAllocationAgeUnit(0)
		, tarokAllocationAgeExponentBase(2.0)
		, tarokUseProjectedSurvivalCollectionSet(true)
		, tarokWorkSplittingPeriod(1024)
		, tarokMinimumGMPWorkTargetBytes()
		, tarokConcurrentMarkingCostWeight(0.05)
		, tarokAutomaticDefragmentEmptinessThreshold(false)
		, tarokEnableCopyForwardHybrid(true)
		, tarokReserveRegionsFromCollectionSet(RESERVE_REGIONS_NO)
		, tarokEnableRecoverRegionTailsAfterSweep(false)
		, tarokTargetMaxPauseTime(200)
#if defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD)
		, _isConcurrentCopyForward(false)
#endif /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */
		, tarokTailCandidateListSortOrder(SORT_ORDER_NOORDER)
#endif /* defined (OMR_GC_VLHGC) */
		, tarokEnableExpensiveAssertions(false)
		, tarokEnableAllocationPointerAssertion(false)
		, sweepPoolManagerAddressOrderedList(NULL)
		, sweepPoolManagerSmallObjectArea(NULL)
		, sweepPoolManagerBumpPointer(NULL)
		, _mainThreadCpuTimeNanos(0)
		, alwaysCallWriteBarrier(false)
		, alwaysCallReadBarrier(false)
		, _holdRandomThreadBeforeHandlingWorkUnit(false)
		, _holdRandomThreadBeforeHandlingWorkUnitPeriod(100)
		, _forceRandomBackoutsAfterScan(false)
		, _forceRandomBackoutsAfterScanPeriod(5)
		, referenceChainWalkerMarkMap(NULL)
		, trackMutatorThreadCategory(false)
		, darkMatterSampleRate(32)
		, pretouchHeapOnExpand(false)
		, decommitMinimumFree(0)
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
		, gcOnIdle(false)
		, compactOnIdle(false)
#endif /* defined(OMR_GC_IDLE_HEAP_MANAGER) */
#if defined(OMR_VALGRIND_MEMCHECK)
		, valgrindMempoolAddr(0)
		, memcheckHashTable(NULL)
		, shouldForceLowMemoryHeapCeilingShiftIfPossible(false)
#endif /* defined(OMR_VALGRIND_MEMCHECK) */
	{
		_typeId = __FUNCTION__;
	}

};
#endif /* GCEXTENSIONSBASE_HPP_ */
