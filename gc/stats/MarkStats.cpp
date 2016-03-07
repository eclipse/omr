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

#include "omrcfg.h"
#include "modronopt.h"

#if defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME)

#include "MarkStats.hpp"

void
MM_MarkStats::clear()
{
	_scanTime = 0;
	
	_objectsMarked = 0;
	_objectsScanned = 0;
	_bytesScanned = 0;

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	_syncStallCount = 0;
	_syncStallTime = 0;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
};

void
MM_MarkStats::merge(MM_MarkStats *statsToMerge)
{
	_scanTime += statsToMerge->_scanTime;

	_objectsMarked += statsToMerge->_objectsMarked;
	_objectsScanned += statsToMerge->_objectsScanned;
	_bytesScanned += statsToMerge->_bytesScanned;

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	/* It may not ever be useful to merge these stats, but do it anyways */
	_syncStallCount += statsToMerge->_syncStallCount;
	_syncStallTime += statsToMerge->_syncStallTime;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
};

#endif /* OMR_GC_MODRON_STANDARD || OMR_GC_REALTIME */
