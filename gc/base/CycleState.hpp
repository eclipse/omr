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

#if !defined(CYCLESTATE_HPP_)
#define CYCLESTATE_HPP_

#include "omrgcconsts.h"

#include "GCCode.hpp"

class MM_CollectionStatistics;
class MM_MarkMap;
class MM_MemorySubSpace;
class MM_WorkPacketsVLHGC;

/**
 * Per cycle state information
 */
class MM_CycleState {
public:
	enum CollectionType {
		CT_GLOBAL_GARBAGE_COLLECTION = 0, /**< Global tracing to create a valid mark map across the entire heap (with possible reclamation) */
		CT_PARTIAL_GARBAGE_COLLECTION = 1, /**< Partial heap trace / reclaim */
		CT_GLOBAL_MARK_PHASE = 2, /**< Global mark phase type (performs a full heap trace to build a fresh mark map) */
	};

	MM_GCCode _gcCode;
	MM_MemorySubSpace* _activeSubSpace;
	MM_CollectionStatistics* _collectionStatistics; /**< Pointer to a particular instance of common collect stats (memory, duration etc) */
	MM_WorkPacketsVLHGC* _workPackets;
	MM_MarkMap* _markMap; /**< Mark map central to activity of the current cycle */

	volatile bool _finalizationRequired; /**< Does the current cycle require finalization upon completion */
	volatile bool _dynamicClassUnloadingEnabled; /**< Determine if dynamic class unloading is enabled for this cycle */

	CollectionType _collectionType; /**< What type of collection the cycle represents */

	enum MarkDelegateState {
		state_mark_idle = 1,
		state_mark_map_init,
		state_initial_mark_roots,
		state_process_work_packets_after_initial_mark,
		state_final_roots_complete,
	} _markDelegateState; /**< The state of the MarkDelegate's incremental state machine (part of the cycle since multiple cycles could be in different places in the machine) */

	struct {
		uintptr_t _survivorSetRegionCount; /**< Count of regions Copy Forward used as destination. Effective usage of regions is accounted, thus the count is not a cardinal number. */
	} _pgcData;

	uintptr_t _currentIncrement; /**< The index of the current increment within the cycle, starting at 0 (used for event reporting) */

	bool _shouldRunCopyForward; /**< True if this cycle is to run a copy-forward-based attempt at reclaiming memory, false if mark-compact is to be used */

	enum ReasonForMarkCompactPGC {
		reason_not_exceptional = 0, /**< Due to a deliberate choice (not because we were forced) */
		reason_JNI_critical_in_Eden = 1, /**< Required since JNI critical regions were in Eden */
		reason_calibration = 2, /**< Required to calibrate Mark rate statistics */
		reason_recent_abort = 3, /**< Playing it safe until a GMP completes since a Copy-Forward abort recently occurred */
		reason_insufficient_free_space = 4, /**< Insufficient free regions for survivor space */
	} _reasonForMarkCompactPGC; /**< Set if JNI critical regions were observed in Eden space, for this cycle */

	MM_CycleState* _externalCycleState; /**< Another state which overlaps with this and needs to be fixed up to reflect any changes to the heap made under this cycle */
	uintptr_t _type;
	uintptr_t _verboseContextID; /**< the verbose context id for this cycle.  This should only be read/written to by verbose gc */

	enum ReferenceObjectOptions {
		references_default = 0x00, /**< default behaviour for all references */
		references_clear_weak = 0x01, /**< clear the referent in any WeakReferences (set after WeakReference processing) */
		references_clear_soft = 0x02, /**< clear the referent in any SoftReferences (set after SoftReference processing) */
		references_clear_phantom = 0x04, /**< clear the referent in any PhantomReferences (set after PhantomReference processing) */
		references_soft_as_weak = 0x08 /**< treat SoftReferences as weak (ignore the age) (set for OOM collections) */
	};

	uintptr_t _referenceObjectOptions; /**< A bitmask controlling how Reference objects are treated during marking */

public:
	MM_CycleState()
		: _gcCode(J9MMCONSTANT_IMPLICIT_GC_DEFAULT)
		, _activeSubSpace(NULL)
		, _collectionStatistics(NULL)
		, _workPackets(NULL)
		, _markMap(NULL)
		, _finalizationRequired(false)
		, _dynamicClassUnloadingEnabled(false)
		, _collectionType(CT_GLOBAL_GARBAGE_COLLECTION)
		, _markDelegateState(state_mark_idle)
		, _currentIncrement(0)
		, _shouldRunCopyForward(false)
		, _reasonForMarkCompactPGC(reason_not_exceptional)
		, _externalCycleState(NULL)
		, _type(OMR_GC_CYCLE_TYPE_DEFAULT)
		, _verboseContextID(0)
		, _referenceObjectOptions(references_default)
	{
	}
};

#endif /* CYCLESTATE_HPP_ */
