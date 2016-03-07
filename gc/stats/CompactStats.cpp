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
 
