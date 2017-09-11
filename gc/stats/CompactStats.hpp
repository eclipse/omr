/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(COMPACTSTATS_HPP_)
#define COMPACTSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"

#if defined(OMR_GC_MODRON_STANDARD)

#if defined(OMR_GC_MODRON_COMPACTION)
#include "Base.hpp"

/**
 * Storage for stats relevant to the compaction phase of a collection.
 * @ingroup GC_Stats
 */
class MM_CompactStats : public MM_Base 
{
public:
	CompactReason _compactReason;
	CompactPreventedReason _compactPreventedReason;

	uintptr_t _movedObjects;
	uintptr_t _movedBytes;
	uintptr_t _fixupObjects;
	uint64_t _setupStartTime;
	uint64_t _setupEndTime;
	uint64_t _moveStartTime;
	uint64_t _moveEndTime;
	uint64_t _fixupStartTime;
	uint64_t _fixupEndTime;
	uint64_t _rootFixupStartTime;
	uint64_t _rootFixupEndTime;
		
	/* Remember gc count on last compaction of heap */
	uintptr_t _lastHeapCompaction;

	uint64_t _startTime;	/**< Compact start time */
	uint64_t _endTime;		/**< Compact end time */

	void clear();
	void merge(MM_CompactStats *statsToMerge);

	MM_CompactStats() :
		MM_Base()
		,_lastHeapCompaction(0)
		,_startTime(0)
		,_endTime(0)
	{
		clear();
	};

};

#endif /* OMR_GC_MODRON_COMPACTION */
#endif /* OMR_GC_MODRON_STANDARD */
#endif /* COMPACTSTATS_HPP_ */
