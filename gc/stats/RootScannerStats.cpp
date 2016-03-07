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

#include "omrcfg.h"
#include "omrcomp.h"

#include "RootScannerStats.hpp"

void
MM_RootScannerStats::clear()
{
	for (uintptr_t i = 0; i < RootScannerEntity_Count; i++) {
		_entityScanTime[i] = 0;
	}
}

void
MM_RootScannerStats::merge(MM_RootScannerStats *statsToMerge)
{
	for (uintptr_t i = 0; i < RootScannerEntity_Count; i++) {
		_entityScanTime[i] += statsToMerge->_entityScanTime[i];
	}
}
