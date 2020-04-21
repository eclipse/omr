/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
	_splitArraysProcessed = 0;
	_splitArraysAmount = 0;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
}

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
	_splitArraysProcessed += statsToMerge->_splitArraysProcessed;
	_splitArraysAmount += statsToMerge->_splitArraysAmount;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
}

#endif /* OMR_GC_MODRON_STANDARD || OMR_GC_REALTIME */
