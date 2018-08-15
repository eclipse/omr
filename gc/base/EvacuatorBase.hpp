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

#ifndef EVACUATORBASE_HPP_
#define EVACUATORBASE_HPP_

#undef EVACUATOR_DEBUG
#undef EVACUATOR_DEBUG_ALWAYS

#if defined(EVACUATOR_DEBUG)
#include "omrgcconsts.h"
#include "ModronAssertions.h"

#include "Math.hpp"

/* base debug flags */
#define EVACUATOR_DEBUG_END 1
#define EVACUATOR_DEBUG_CYCLE 2
#define EVACUATOR_DEBUG_EPOCH 4
#define EVACUATOR_DEBUG_WORK 8
#define EVACUATOR_DEBUG_STACK 16
#define EVACUATOR_DEBUG_COPY 32
#define EVACUATOR_DEBUG_ALLOCATE 64
#define EVACUATOR_DEBUG_WHITELISTS 128
#define EVACUATOR_DEBUG_POISON_DISCARD 256
#define EVACUATOR_DEBUG_BACKOUT 512
#define EVACUATOR_DEBUG_DELEGATE 1024

/* default debug flags */
#define EVACUATOR_DEBUG_DEFAULT_FLAGS (0)

/* delegate can define additional flags above 0x10000 */
#define EVACUATOR_DEBUG_DELEGATE_BASE 0x10000

#define Debug_MM_true(assertion) Assert_MM_true(assertion)
#define Debug_MM_true1(env, assertion, format, arg) Assert_GC_true_with_message1(env, assertion, format, arg)
#define Debug_MM_true2(env, assertion, format, arg1, arg2) Assert_GC_true_with_message2(env, assertion, format, arg1, arg2)
#define Debug_MM_true3(env, assertion, format, arg1, arg2, arg3) Assert_GC_true_with_message3(env, assertion, format, arg1, arg2, arg3)
#define Debug_MM_true4(env, assertion, format, arg1, arg2, arg3, arg4) Assert_GC_true_with_message4(env, assertion, format, arg1, arg2, arg3, arg4)
#else
#define Debug_MM_true(assertion)
#define Debug_MM_true1(env, assertion, format, arg)
#define Debug_MM_true2(env, assertion, format, arg1, arg2)
#define Debug_MM_true3(env, assertion, format, arg1, arg2, arg3)
#define Debug_MM_true4(env, assertion, format, arg1, arg2, arg3, arg4)
#endif /* defined(EVACUATOR_DEBUG) */

#include "GCExtensionsBase.hpp"

/* ensure that there is a reasonable range between min/max whitespace allocation sizes */
#if (DEFAULT_SCAN_CACHE_MINIMUM_SIZE > (DEFAULT_SCAN_CACHE_MAXIMUM_SIZE >> 3))
#error "Scan cache default sizes must satisfy DEFAULT_SCAN_CACHE_MINIMUM_SIZE <= (DEFAULT_SCAN_CACHE_MAXIMUM_SIZE >> 3)"
#endif /* (DEFAULT_SCAN_CACHE_MINIMUM_SIZE > (DEFAULT_SCAN_CACHE_MAXIMUM_SIZE >> 3)) */

class MM_EvacuatorBase
{
/**
 * Data members
 */
private:
#if defined(EVACUATOR_DEBUG)
	uintptr_t _gcCycle;
	uintptr_t _gcEpoch;
	uintptr_t _debugCycle;
	uintptr_t _debugEpoch;
	uintptr_t _debugTrace;
	uintptr_t _debugFlags;
#endif /* defined(EVACUATOR_DEBUG) */

protected:
public:
	/* lower bound for work queue volume -- lower values trigger clipping of whitespace tlh allocation sizes */
	static const uintptr_t low_work_volume = MINIMUM_TLH_SIZE;

	/* hard bounds for TLH allocation size */
	static const uintptr_t min_tlh_allocation_size = DEFAULT_SCAN_CACHE_MINIMUM_SIZE;
	static const uintptr_t max_tlh_allocation_size = DEFAULT_SCAN_CACHE_MAXIMUM_SIZE;
	static const uintptr_t tlh_allocation_granularity = 4096;

	/* hard bounds for work packet size */
	static const uintptr_t min_work_packet_size = min_tlh_allocation_size << 1;
	static const uintptr_t max_work_packet_size = max_tlh_allocation_size >> 1;

	/* largest amount of whitespace that can be discarded from the scan stack and outside copyspaces */
	static const uintptr_t max_scanspace_remainder = 32;
	static const uintptr_t max_copyspace_remainder = MINIMUM_TLH_SIZE;

	/* maximum number of array element slots to include in each split array segment */
	static const uintptr_t max_split_segment_elements = DEFAULT_ARRAY_SPLIT_MINIMUM_SIZE;
	/* maximum size in bytes of array slots in each split array segment */
	static const uintptr_t max_split_segment_size = max_split_segment_elements * sizeof(fomrobject_t);
	/* minimum size in bytes of a splitable indexable object */
	static const uintptr_t min_split_indexable_size = 2 * max_split_segment_size;

	/* Actual maximal size of scan stack -- operational limit may be lowered to increase outside copying */
	static const uintptr_t max_scan_stack_depth = 32;
	/* Object size threshold for copying inside -- larger objects are always copied to outside copyspaces */
	static const uintptr_t max_inside_object_size = 256;

	/* base 2 log of upper bound on distance from base to copy head in stack scan frame*/
	static const uintptr_t inside_copy_log_size = 12;
	/* upper bound on distance from base to copy head */
	static const uintptr_t inside_copy_size = (uintptr_t)1 << inside_copy_log_size;
	/* used to set actual bound on copy head at next page boundary within stack scan frame */
	static const uintptr_t inside_copy_mask = inside_copy_size - 1;

	/* number of elements in whitelist backing array must be (2^N)-1 for some N */
	static const uintptr_t max_whitelist = 15;

	enum { always, until, at, from };

/**
 * Function members
 */
private:
protected:
public:
#if defined(EVACUATOR_DEBUG)
	static const char *callsite(const char *id);

	MMINLINE void
	setDebugFlags(uintptr_t debugFlags, uintptr_t debugCycle, uintptr_t debugEpoch, uintptr_t debugTrace = always)
	{
		_debugFlags = debugFlags;
		_debugCycle = debugCycle;
		_debugEpoch = debugEpoch;
		_debugTrace = debugTrace;
	}
	void setDebugFlags(uint64_t debugFlags = EVACUATOR_DEBUG_DEFAULT_FLAGS);

	MMINLINE void setDebugCycleAndEpoch(uintptr_t gcCycle, uintptr_t cycleEpoch) { _gcCycle = gcCycle;  _gcEpoch = cycleEpoch; }

	MMINLINE bool
	isDebugCycleAndEpoch()
	{
		if (0 != _debugFlags) {
			if (always != _debugTrace) {
				switch (_debugTrace) {
				case until:
					return ((0 == _debugCycle) || (_gcCycle <= _debugCycle)) && ((0 == _debugEpoch) || (_gcEpoch <= _debugEpoch));
				case at:
					return ((0 == _debugCycle) || (_gcCycle == _debugCycle)) && ((0 == _debugEpoch) || (_gcEpoch == _debugEpoch));
				case from:
					return ((0 == _debugCycle) || (_gcCycle >= _debugCycle)) && ((0 == _debugEpoch) || (_gcEpoch >= _debugEpoch));
				default:
					break;
				}
			}
		}
		return true;
	}

	MMINLINE bool isDebugFlagSet(uintptr_t debugFlag) { return isDebugCycleAndEpoch() && (debugFlag == (debugFlag & _debugFlags)); }
	MMINLINE bool isAnyDebugFlagSet(uintptr_t debugFlags) { return isDebugCycleAndEpoch() && (0 != (debugFlags | _debugFlags)); }
	MMINLINE bool areAllDebugFlagsSet(uintptr_t debugFlags) { return isDebugFlagSet(debugFlags); }
	MMINLINE bool isDebugEnd() { return isDebugFlagSet(EVACUATOR_DEBUG_END); }
	MMINLINE bool isDebugCycle() { return isDebugFlagSet(EVACUATOR_DEBUG_CYCLE); }
	MMINLINE bool isDebugEpoch() { return isDebugFlagSet(EVACUATOR_DEBUG_EPOCH); }
	MMINLINE bool isDebugStack() { return isDebugFlagSet(EVACUATOR_DEBUG_STACK); }
	MMINLINE bool isDebugWork() { return isDebugFlagSet(EVACUATOR_DEBUG_WORK); }
	MMINLINE bool isDebugCopy() { return isDebugFlagSet(EVACUATOR_DEBUG_COPY); }
	MMINLINE bool isDebugWhitelists() { return isDebugFlagSet(EVACUATOR_DEBUG_WHITELISTS); }
	MMINLINE bool isDebugPoisonDiscard() { return isDebugFlagSet(EVACUATOR_DEBUG_POISON_DISCARD); }
	MMINLINE bool isDebugAllocate() { return isDebugFlagSet(EVACUATOR_DEBUG_ALLOCATE); }
	MMINLINE bool isDebugBackout() { return isDebugFlagSet(EVACUATOR_DEBUG_BACKOUT); }
	MMINLINE bool isDebugDelegate() { return isDebugFlagSet(EVACUATOR_DEBUG_DELEGATE); }
#else
	MMINLINE void setDebugFlags(uintptr_t debugFlags, uintptr_t debugCycle, uintptr_t debugEpoch, uintptr_t debugTrace = 0) { }
	MMINLINE void setDebugFlags(uint64_t debugFlags = 0) { }
	MMINLINE void setDebugCycleAndEpoch(uintptr_t gcCycle, uintptr_t cycleEpoch) { }
	MMINLINE bool isDebugCycleAndEpoch() { return false; }
	MMINLINE bool isDebugFlagSet(uintptr_t debugFlag) { return false; }
	MMINLINE bool isAnyDebugFlagSet(uintptr_t debugFlags) { return false; }
	MMINLINE bool areAllDebugFlagsSet(uintptr_t debugFlags) { return false; }
	MMINLINE bool isDebugEnd() { return false; }
	MMINLINE bool isDebugCycle() { return false; }
	MMINLINE bool isDebugEpoch() { return false; }
	MMINLINE bool isDebugWork() { return false; }
	MMINLINE bool isDebugStack() { return false; }
	MMINLINE bool isDebugCopy() { return false; }
	MMINLINE bool isDebugWhitelists() { return false; }
	MMINLINE bool isDebugPoisonDiscard() { return false; }
	MMINLINE bool isDebugAllocate() { return false; }
	MMINLINE bool isDebugBackout() { return false; }
	MMINLINE bool isDebugDelegate() { return false; }
#endif /* defined(EVACUATOR_DEBUG) */

	MM_EvacuatorBase()
#if defined(EVACUATOR_DEBUG)
	: _gcCycle(0)
	, _gcEpoch(0)
	, _debugCycle(0)
	, _debugEpoch(0)
	, _debugTrace(0)
	, _debugFlags(0)
#endif /* defined(EVACUATOR_DEBUG) */
	{ }
};

#endif /* EVACUATORBASE_HPP_ */
