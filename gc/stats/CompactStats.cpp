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

#include "CompactStats.hpp"

#if defined(OMR_GC_MODRON_STANDARD)

#if defined(OMR_GC_MODRON_COMPACTION)
void
MM_CompactStats::clear() 
{
	_compactReason = COMPACT_NONE;
	_compactPreventedReason = COMPACT_PREVENTED_NONE;
	
	_movedObjects = 0;
	_movedBytes = 0;
	
	_fixupObjects = 0;
	_setupStartTime = 0;
	_setupEndTime = 0;
	_moveStartTime = 0;
	_moveEndTime = 0;
	_fixupStartTime = 0;
	_fixupEndTime = 0;
	_rootFixupStartTime = 0;
	_rootFixupEndTime = 0;
};

void
MM_CompactStats::merge(MM_CompactStats *statsToMerge) 
{
	_movedObjects += statsToMerge->_movedObjects;
	_movedBytes += statsToMerge->_movedBytes;
	_fixupObjects += statsToMerge->_fixupObjects;
	/* merging time intervals is a little different than just creating a total since the sum of two time intervals, for our uses, is their union (as opposed to the sum of two time spans, which is their sum) */
	_setupStartTime = (0 == _setupStartTime) ? statsToMerge->_setupStartTime : OMR_MIN(_setupStartTime, statsToMerge->_setupStartTime);
	_setupEndTime = OMR_MAX(_setupEndTime, statsToMerge->_setupEndTime);
	_moveStartTime = (0 == _moveStartTime) ? statsToMerge->_moveStartTime : OMR_MIN(_moveStartTime, statsToMerge->_moveStartTime);
	_moveEndTime = OMR_MAX(_moveEndTime, statsToMerge->_moveEndTime);
	_fixupStartTime = (0 == _fixupStartTime) ? statsToMerge->_fixupStartTime : OMR_MIN(_fixupStartTime, statsToMerge->_fixupStartTime);
	_fixupEndTime = OMR_MAX(_fixupEndTime, statsToMerge->_fixupEndTime);
	_rootFixupStartTime = (0 == _rootFixupStartTime) ? statsToMerge->_rootFixupStartTime : OMR_MIN(_rootFixupStartTime, statsToMerge->_rootFixupStartTime);
	_rootFixupEndTime = OMR_MAX(_rootFixupEndTime, statsToMerge->_rootFixupEndTime);
};

#endif /* OMR_GC_MODRON_COMPACTION */
#endif /* OMR_GC_MODRON_STANDARD */
 
