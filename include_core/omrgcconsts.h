/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2017
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

#if !defined(OMRGCCONSTS_H_)
#define OMRGCCONSTS_H_

/*
 * @ddr_namespace: default
 */

#include "omrcfg.h"
#include "omrcomp.h"

#define OMR_GC_POLICY_BALANCED 0x4
#define OMR_GC_POLICY_GENCON 0x3
#define OMR_GC_POLICY_ILLEGAL 0x0
#define OMR_GC_POLICY_METRONOME 0x5
#define OMR_GC_POLICY_OPTAVGPAUSE 0x2
#define OMR_GC_POLICY_OPTTHRUPUT 0x1

/*
 * list of available GC policies
 */
typedef enum MM_GCPolicy {
	gc_policy_undefined = OMR_GC_POLICY_ILLEGAL,
	gc_policy_optthruput = OMR_GC_POLICY_OPTTHRUPUT,
	gc_policy_optavgpause = OMR_GC_POLICY_OPTAVGPAUSE,
	gc_policy_gencon = OMR_GC_POLICY_GENCON,
	gc_policy_balanced = OMR_GC_POLICY_BALANCED,
	gc_policy_metronome = OMR_GC_POLICY_METRONOME
} MM_GCPolicy;

#define OMR_GC_WRITE_BARRIER_TYPE_ILLEGAL 0x0
#define OMR_GC_WRITE_BARRIER_TYPE_NONE 0x1
#define OMR_GC_WRITE_BARRIER_TYPE_ALWAYS 0x2
#define OMR_GC_WRITE_BARRIER_TYPE_OLDCHECK 0x3
#define OMR_GC_WRITE_BARRIER_TYPE_CARDMARK 0x4
#define OMR_GC_WRITE_BARRIER_TYPE_CARDMARK_INCREMENTAL 0x5
#define OMR_GC_WRITE_BARRIER_TYPE_CARDMARK_AND_OLDCHECK 0x6
#define OMR_GC_WRITE_BARRIER_TYPE_REALTIME 0x7
#define OMR_GC_WRITE_BARRIER_TYPE_COUNT 0x8

#define OMR_GC_READ_BARRIER_TYPE_ILLEGAL 0x0
#define OMR_GC_READ_BARRIER_TYPE_NONE 0x1
#define OMR_GC_READ_BARRIER_TYPE_ALWAYS 0x2
#define OMR_GC_READ_BARRIER_TYPE_EVACUATE 0x3
#define OMR_GC_READ_BARRIER_TYPE_COUNT 0x4

typedef enum MM_GCWriteBarrierType {
	gc_modron_wrtbar_illegal = OMR_GC_WRITE_BARRIER_TYPE_ILLEGAL,
	gc_modron_wrtbar_none = OMR_GC_WRITE_BARRIER_TYPE_NONE,
	gc_modron_wrtbar_always = OMR_GC_WRITE_BARRIER_TYPE_ALWAYS,
	gc_modron_wrtbar_oldcheck = OMR_GC_WRITE_BARRIER_TYPE_OLDCHECK,
	gc_modron_wrtbar_cardmark = OMR_GC_WRITE_BARRIER_TYPE_CARDMARK,
	gc_modron_wrtbar_cardmark_incremental = OMR_GC_WRITE_BARRIER_TYPE_CARDMARK_INCREMENTAL,
	gc_modron_wrtbar_cardmark_and_oldcheck = OMR_GC_WRITE_BARRIER_TYPE_CARDMARK_AND_OLDCHECK,
	gc_modron_wrtbar_realtime = OMR_GC_WRITE_BARRIER_TYPE_REALTIME,
	gc_modron_wrtbar_count = OMR_GC_WRITE_BARRIER_TYPE_COUNT
} MM_GCWriteBarrierType;

typedef enum MM_GCReadBarrierType {
	gc_modron_readbar_illegal = OMR_GC_READ_BARRIER_TYPE_ILLEGAL,
	gc_modron_readbar_none = OMR_GC_READ_BARRIER_TYPE_NONE,
	gc_modron_readbar_evacuate = OMR_GC_READ_BARRIER_TYPE_EVACUATE,
	gc_modron_readbar_always = OMR_GC_READ_BARRIER_TYPE_ALWAYS,
	gc_modron_readbar_count = OMR_GC_READ_BARRIER_TYPE_COUNT
} MM_GCReadBarrierType;

typedef enum MM_AlignmentType {
	mm_heapAlignment = 1,
	mm_regionAlignment
} MM_AlignmentType;

#define OMR_GC_CYCLE_TYPE_DEFAULT     0
#define OMR_GC_CYCLE_TYPE_GLOBAL      1
#define OMR_GC_CYCLE_TYPE_SCAVENGE    2

/* Core allocation flags defined for OMR are < OMR_GC_ALLOCATE_OBJECT_LANGUAGE_DEFINED_BASE */
#define OMR_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE 0x0
#define OMR_GC_ALLOCATE_OBJECT_INSTRUMENTABLE 0x1
#define OMR_GC_ALLOCATE_OBJECT_TENURED 0x2
#define OMR_GC_ALLOCATE_OBJECT_HASHED 0x8
#define OMR_GC_ALLOCATE_OBJECT_NON_ZERO_TLH 0x10
#define OMR_GC_ALLOCATE_OBJECT_NO_GC 0x20
#define OMR_GC_ALLOCATE_OBJECT_INDEXABLE 0x40
/* Languages may define additional allocation flags >= OMR_GC_ALLOCATE_OBJECT_LANGUAGE_DEFINED_BASE */
#define OMR_GC_ALLOCATE_OBJECT_LANGUAGE_DEFINED_BASE 0x10000
/* Language-defined allocation flags must fit in a uintptr_t */
#define OMR_GC_ALLOCATE_OBJECT_LANGUAGE_DEFINED_MAX ((uintptr_t)(1 << ((8 * sizeof(uintptr_t)) - 1))

#define OMR_GC_ALLOCATION_TYPE_ILLEGAL 0x0
#define OMR_GC_ALLOCATION_TYPE_TLH 0x1
#define OMR_GC_ALLOCATION_TYPE_SEGREGATED 0x2
#define OMR_GC_ALLOCATION_TYPE_COUNT 0x3

typedef enum MM_GCAllocationType {
	gc_modron_allocation_type_illegal = OMR_GC_ALLOCATION_TYPE_ILLEGAL,
	gc_modron_allocation_type_tlh = OMR_GC_ALLOCATION_TYPE_TLH,
	gc_modron_allocation_type_segregated = OMR_GC_ALLOCATION_TYPE_SEGREGATED,
	gc_modron_allocation_type_count = OMR_GC_ALLOCATION_TYPE_COUNT,
} MM_GCAllocationType;

/* Object-model related constants moved from builder (J9VMConstantValue.st / j9generated.h)
 * These replace the following constants from builder (J9VMConstantValue.st / j9consts.h):
 * #define J9_OBJECT_HEADER_AGE_MAX 0xE
 * #define J9_OBJECT_HEADER_AGE_MIN 0x1
 * Note that some related defines remain, in both j9generated.h and j9consts.h, e.g.:
 * J9_OBJECT_HEADER_AGE_DEFAULT		/ OBJECT_HEADER_AGE_DEFAULT
 * J9_OBJECT_HEADER_INDEXABLE		/ OBJECT_HEADER_INDEXABLE
 * J9_OBJECT_HEADER_SHAPE_MASK		/ OBJECT_HEADER_SHAPE_MASK
 * J9_OBJECT_HEADER_SHAPE_MASK		/ OBJECT_HEADER_SHAPE_MASK
 * J9_OBJECT_HEADER_STACK_ALLOCATED	/ OBJECT_HEADER_STACK_ALLOCATED
 * */
#define OBJECT_HEADER_AGE_MIN  1
#define OBJECT_HEADER_AGE_MAX  14

/**
 * #defines representing tags used in the Remembered Set
 */
#define DEFERRED_RS_REMOVE_FLAG 			0x1

#define J9_MU_WALK_DEBUGGER_REFS 0x1
#define J9_MU_WALK_DEBUGGER_CLASS_REFS 0x2
#define J9_MU_WALK_ALL 0x3
#define J9_MU_WALK_OBJECT_BASE 0x4
#define J9_MU_WALK_SKIP_JVMTI_TAG_TABLES 0x20
#define J9_MU_WALK_TRACK_VISIBLE_FRAME_DEPTH 0x40
#define J9_MU_WALK_IGNORE_CLASS_SLOTS 0x80
#define J9_MU_WALK_NEW_AND_REMEMBERED_ONLY 0x10
#define J9_MU_WALK_IGNORE_NULL_ARRAYLET_LEAF 0x100
#define J9_MU_WALK_PREINDEX_INTERFACE_FIELDS 0x200

/* When changing the heap alignment below you need to check that the new alignment
 * is compatible with the requirements of compaction and concurrent mark.
 */
#if defined(OMR_ENV_DATA64)
#define HEAP_ALIGNMENT 1024
#else /* !OMR_ENV_DATA64 */
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#define HEAP_ALIGNMENT 512
#else /* !OMR_GC_MODRON_CONCURRENT_MARK */
#define HEAP_ALIGNMENT 256
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#endif /* OMR_ENV_DATA64 */

#define MINIMUM_VM_SIZE ((uintptr_t) 1*1024*1024)
#define MINIMUM_OLD_SPACE_SIZE ((uintptr_t) (MINIMUM_VM_SIZE/2))
#define MINIMUM_NEW_SPACE_SIZE ((uintptr_t) (MINIMUM_VM_SIZE/4))

#define NO_FRAGMENTATION 0
#define MICRO_FRAGMENTATION 1
#define MACRO_FRAGMENTATION 2

/**
 * Lock initialization and option structure.
 * Used to describe how locks should be initialized when created.
 */
struct ModronLnrlOptions {
	uintptr_t spinCount1;
	uintptr_t spinCount2;
	uintptr_t spinCount3;
};

/* Flag used to poison collected object pointers for debugging */
#define J9_INVALID_OBJECT ((omrobjectptr_t)UDATA_MAX)

#define OMRPORT_ACCESS_FROM_ENVIRONMENT(env) OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary())

#if defined(DEBUG)
#define MMINLINE
#else /* DEBUG */
#if defined(WIN32)
#define MMINLINE __forceinline
#elif (__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
#define MMINLINE inline __attribute((always_inline))
#else /* WIN32 */
#define MMINLINE inline
#endif /* WIN32 */
#endif /* DEBUG */

#if defined(WIN32)
#define MMINLINE_DEBUG __forceinline
#elif ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#define MMINLINE_DEBUG inline __attribute((always_inline))
#else /* WIN32 */
#define MMINLINE_DEBUG inline
#endif /* WIN32 */

/**
 * Lightweight Non-Reentrant Locks (LWNR) Spinlock Support
 * We can't use spinlocks on platforms that do not support semaphores.
 * Also, spinlocks can't provide realtime locking.
 * Do not use the custom spinlocks on AIX as yield calls create performance issues.
 */
#if defined(OMR_INTERP_HAS_SEMAPHORES) && !defined(AIXPPC)
#define J9MODRON_USE_CUSTOM_SPINLOCKS
#endif

#define J9MODRON_USE_CUSTOM_READERWRITERLOCK

/* Since only 1 bit is tagged, we only need to shift 1 bit.
 * These two defines are tightly coupled and should be modified together.
 */
#define PACKET_ARRAY_SPLIT_TAG 1
/*
 * if CURRENT_UNIT_ONLY_TAG bit is clear, the work unit is from index till the end of the array (normal case)
 * if CURRENT_UNIT_ONLY_TAG bit is set, the work unit is from the index with size only of arraySplitSize (exceptional case)
 */
#define PACKET_ARRAY_SPLIT_CURRENT_UNIT_ONLY_TAG 2
/* we are not worried about shifting index by 2:
 * - for 64 bit platforms, for obvious resoans (index is < 2^31)
 * - for 32 bit platforms, no reference array of size more than 2^30 can be created, since each ref slot is 4 bytes
 * 	 (unless we use slot compression, which we do not for now)
 */
#define PACKET_ARRAY_SPLIT_SHIFT 2
#define PACKET_INVALID_OBJECT (UDATA_MAX << PACKET_ARRAY_SPLIT_SHIFT)

#define OLDFREE_DESPERATE_RATIO_DIVISOR			100
#define OLDFREE_DESPERATE_RATIO_MULTIPLIER		4
#define OLDFREE_INSUFFICIENT					((uintptr_t)(1024 * 128))
#define MINIMUM_TLHSIZE_MULTIPLIER				2
#define MINIMUM_CONTRACTION_RATIO_DIVISOR		100
#define MINIMUM_CONTRACTION_RATIO_MULTIPLIER	10

#define DESIRED_SUBAREA_SIZE		((uintptr_t)(4*1024*1024))

typedef enum {
	COMPACT_NONE = 0,
	COMPACT_LARGE = 1,
	COMPACT_FRAGMENTED = 2,
	COMPACT_FORCED_GC = 4,
	COMPACT_AVOID_DESPERATE = 5,
	COMPACT_MEMORY_INSUFFICIENT = 6,
	COMPACT_ALWAYS = 7,
	COMPACT_CONTRACT = 11,
	COMPACT_AGGRESSIVE= 12
} CompactReason;

typedef enum {
	COMPACT_PREVENTED_NONE = 0,
	COMPACT_PREVENTED_CRITICAL_REGIONS
} CompactPreventedReason;

/**
 * @}
 */

/**
 * Concurrent status symbols. Client language may define additional status values > CONCURRENT_ROOT_TRACING
 * and < CONCURRENT_TRACE_ONLY corresponding to parallelizable units of tracing work.
 * @ingroup GC_Base_Core
 * @{
 */
typedef enum {
	CONCURRENT_OFF = 1,
	CONCURRENT_INIT_RUNNING,
	CONCURRENT_INIT_COMPLETE,
	CONCURRENT_ROOT_TRACING,
	/* ^ Value range reserved for client language v */
	CONCURRENT_TRACE_ONLY = 28,
	CONCURRENT_CLEAN_TRACE = 29,
	CONCURRENT_EXHAUSTED = 30,
	CONCURRENT_FINAL_COLLECTION = 31
} ConcurrentStatus;

/**
 * Concurrent status symbols
 * Used explain why kickoff triggered
 * @ingroup GC_Base_Core
 * @{
 */
typedef enum {
	NO_KICKOFF_REASON=1,
	KICKOFF_THRESHOLD_REACHED,
	NEXT_SCAVENGE_WILL_PERCOLATE,
	LANGUAGE_DEFINED_REASON
} ConcurrentKickoffReason;

/**
 * Concurrent status symbols.
 * Client languages may define additional values > NO_LANGUAGE_KICKOFF_REASON
 * to explain why a kickoff was triggered.
 */
typedef enum {
	NO_LANGUAGE_KICKOFF_REASON=1,
} ConcurrentKickoffLanguageReason;

/**
 * Concurrent status symbols
 * @ingroup GC_Base_Core
 * @{
 */
typedef enum {
	ABORT_COLLECTION_INSUFFICENT_PROGRESS=1,
	ABORT_COLLECTION_REMEMBERSET_OVERFLOW,
	ABORT_COLLECTION_SCAVENGE_REMEMBEREDSET_OVERFLOW,
	ABORT_COLLECTION_PREPARE_HEAP_FOR_WALK
} CollectionAbortReason;

/**
 * Concurrent status symbols.
 * Used explain why tracing stop and card cleaning began
 * @ingroup GC_Base_Core
 * @{
 */
 typedef enum {
		CARD_CLEANING_REASON_NONE=0,
		TRACING_COMPLETED,
		CARD_CLEANING_THRESHOLD_REACHED
} ConcurrentCardCleaningReason;


#if defined(OMR_GC_REALTIME)
/* Reasons we are doing some GC work */
typedef enum {
	UNKOWN_REASON = 0,
	TIME_TRIGGER,
	WORK_TRIGGER,
	OUT_OF_MEMORY_TRIGGER,
	SYSTEM_GC_TRIGGER,
	VM_SHUTDOWN
} GCReason;
#endif /* OMR_GC_REALTIME */

/**
 * Percolate reason symbols
 * @ingroup GC_Base_Core
 * @{
 */
typedef enum {
	NONE_SET = 1,
	INSUFFICIENT_TENURE_SPACE,
	FAILED_TENURE,
	MAX_SCAVENGES,
	RS_OVERFLOW,
	UNLOADING_CLASSES,
	EXPAND_FAILED,
	CRITICAL_REGIONS
} PercolateReason;
/**
 * @}
 */

typedef enum {
	HEAP_NO_RESIZE,
	HEAP_EXPAND,
	HEAP_CONTRACT,
	HEAP_LOA_EXPAND,
	HEAP_LOA_CONTRACT
} HeapResizeType;

typedef enum {
	NO_CONTRACT = 1,
	GC_RATIO_TOO_LOW,
	FREE_SPACE_GREATER_MAXF,
	SCAV_RATIO_TOO_LOW,
	HEAP_RESIZE,
	SATISFY_EXPAND,
	FORCED_NURSERY_CONTRACT
} ContractReason;

typedef enum {
	NO_EXPAND = 1,
	GC_RATIO_TOO_HIGH,
	FREE_SPACE_LESS_MINF,
	SCAV_RATIO_TOO_HIGH,
	SATISFY_COLLECTOR,
	EXPAND_DESPERATE,
	FORCED_NURSERY_EXPAND
} ExpandReason;

typedef enum {
	NO_LOA_RESIZE = 1,
	LOA_EXPAND_FAILED_ALLOCATE,
	LOA_EXPAND_LAST_RESIZE_REASON = LOA_EXPAND_FAILED_ALLOCATE,
	LOA_CONTRACT_AGGRESSIVE,
	LOA_CONTRACT_MIN_SOA,
	LOA_CONTRACT_UNDERUTILIZED,
	LOA_CONTRACT_LAST_RESIZE_REASON = LOA_CONTRACT_UNDERUTILIZED
} LoaResizeReason;

typedef enum {
	FIXUP_NONE = 0,
	FIXUP_CLASS_UNLOADING,
	FIXUP_DEBUG_TOOLING
} FixUpReason;

typedef enum {
	NOT_REQUIRED = 1,
	ABOUT_TO_GC,
	COMPACTION_REQUIRED,
	CONTRACTION_REQUIRED,
	EXPANSION_REQUIRED,
	LOA_RESIZE,
	SYSTEM_GC
} SweepCompletionReason;

/**
 * @ingroup GC_Include
 * @name Cycle type flags.
 * Currently the "extended" set above those found in omrgcconsts.h - on the VM side.  Those constants should be moved back over to this
 * side and everything unified.  Until then, continue the list here.
 * @{
 */
#define OMR_GC_CYCLE_TYPE_VLHGC_PARTIAL_GARBAGE_COLLECT 3
#define OMR_GC_CYCLE_TYPE_VLHGC_GLOBAL_MARK_PHASE 4
#define OMR_GC_CYCLE_TYPE_VLHGC_GLOBAL_GARBAGE_COLLECT 5
/** @} */

/* Object head object / hole bits */
#ifndef J9_GC_OBJ_HEAP_HOLE
#define J9_GC_OBJ_HEAP_HOLE 0x1
#endif
#ifndef J9_GC_OBJ_HEAP_HOLE_MASK
#define J9_GC_OBJ_HEAP_HOLE_MASK 0x3
#endif
#ifndef J9_GC_MULTI_SLOT_HOLE
#define J9_GC_MULTI_SLOT_HOLE 0x1
#endif
#ifndef J9_GC_SINGLE_SLOT_HOLE
#define J9_GC_SINGLE_SLOT_HOLE 0x3
#endif

#define J9VMGC_SIZECLASSES_LOG_SMALLEST 0x4

/* Default object age for generational collectors */
#define J9_OBJECT_HEADER_AGE_DEFAULT 0xA

#define J9_SCV_TENURE_RATIO_LOW 10
#define J9_SCV_TENURE_RATIO_HIGH 30
#define J9_SCV_REMSET_MAX 65536
#define J9_SCV_REMSET_FRAGMENT_SIZE 32
#define J9_SCV_REMSET_SIZE 16384

#define J9MODRON_ALLOCATION_MANAGER_HINT_MAX_WALK 20

/* Define the low memory heap ceiling (max heap address when -Xgc:forceLowMemHeap is specified) */
#if defined(OMR_ENV_DATA64)
/* highest supported shift for the low memory setting */
#define LOW_MEMORY_HEAP_CEILING_SHIFT 4
/* shift required for the "default" low memory setting */
#define DEFAULT_LOW_MEMORY_HEAP_CEILING_SHIFT 3
/* 4 GiB when running scaling heap-base 0 VMs */
#define NON_SCALING_LOW_MEMORY_HEAP_CEILING ((uintptr_t)1 << 32)
/* 64 GiB when running scaling heap-base 0 VMs */
#define LOW_MEMORY_HEAP_CEILING (NON_SCALING_LOW_MEMORY_HEAP_CEILING << LOW_MEMORY_HEAP_CEILING_SHIFT)
#define DEFAULT_LOW_MEMORY_HEAP_CEILING (NON_SCALING_LOW_MEMORY_HEAP_CEILING << DEFAULT_LOW_MEMORY_HEAP_CEILING_SHIFT)
#else /* defined(OMR_ENV_DATA64) */
#define LOW_MEMORY_HEAP_CEILING ((uintptr_t)0)
#define NON_SCALING_LOW_MEMORY_HEAP_CEILING ((uintptr_t)0)
#endif /* defined(OMR_ENV_DATA64) */

/* Because SLES zLinux/31 never allocates mmap()ed memory below the 1GB line unless you ask it to, we
 * always request that the heap is allocated low in the address range. This leaves the space above
 * 2GB free for other mmap() allocations (e.g. pthread stacks).*/
#if defined(S390) && defined(LINUX) && !defined(OMR_ENV_DATA64)
#define PREFERRED_HEAP_BASE 0x10000000
#else
#define PREFERRED_HEAP_BASE 0x0
#endif

#if defined(OMR_GC_COMPRESSED_POINTERS)
#define SUBALLOCATOR_INITIAL_SIZE (200*1024*1024)
#define SUBALLOCATOR_COMMIT_SIZE (50*1024*1024)
#if defined(AIXPPC)
/* virtual memory is assigned in segment of 256M, so grab the entire segment */
#define SUBALLOCATOR_ALIGNMENT (256*1024*1024)
#else /* defined(AIXPPC) */
#define SUBALLOCATOR_ALIGNMENT (8*1024*1024)
#endif /* defined(AIXPPC) */
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */

#if defined(OMR_GC_REALTIME)
#define METRONOME_DEFAULT_HRT_PERIOD_MICRO 1000 /* This gives vanilla linux a chance to use the HRT */
#define METRONOME_DEFAULT_BEAT_MICRO 3000
#define METRONOME_DEFAULT_TIME_WINDOW_MICRO 60000
#endif /* OMR_GC_REALTIME */

#endif /* OMRGCCONSTS_H_ */
