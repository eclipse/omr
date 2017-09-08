/*******************************************************************************
 * Copyright (c) 2010, 2015 IBM Corp. and others
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

#include "spacesaving.h"


OMRSpaceSaving *
spaceSavingNew(OMRPortLibrary *portLibrary, uint32_t size)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	OMRSpaceSaving *newSpaceSaving = omrmem_allocate_memory(sizeof(OMRSpaceSaving), OMRMEM_CATEGORY_MM);
	if (NULL == newSpaceSaving) {
		return NULL;
	}
	newSpaceSaving->portLib = portLibrary;
	newSpaceSaving->ranking = rankingNew(portLibrary, size);
	if (NULL == newSpaceSaving->ranking) {
		return NULL;
	}
	return newSpaceSaving;
}

void
spaceSavingClear(OMRSpaceSaving *spaceSaving)
{
	rankingClear(spaceSaving->ranking);
}

/* Todo: Implement capability to tell when the algorithm isn't performing well
 * Can do this by checking how often certain entries in ranking get clobbered
 */
void
spaceSavingUpdate(OMRSpaceSaving *spaceSaving, void *data, uintptr_t count)
{
	if (rankingIncrementEntry(spaceSaving->ranking, data, count) != TRUE) { /* doesn't exist in ranking*/
		if (spaceSaving->ranking->curSize == spaceSaving->ranking->size) {
			rankingUpdateLowest(spaceSaving->ranking, data, rankingGetLowestCount(spaceSaving->ranking) + count);
		} else {
			rankingUpdateLowest(spaceSaving->ranking, data, count);
		}
	}
}

void
spaceSavingFree(OMRSpaceSaving *spaceSaving)
{
	OMRPORT_ACCESS_FROM_OMRPORT(spaceSaving->portLib);
	rankingFree(spaceSaving->ranking);
	omrmem_free_memory(spaceSaving);
	return;
}

void *
spaceSavingGetKthMostFreq(OMRSpaceSaving *spaceSaving, uintptr_t k)
{
	return rankingGetKthHighest(spaceSaving->ranking, k);
}

uintptr_t
spaceSavingGetKthMostFreqCount(OMRSpaceSaving *spaceSaving, uintptr_t k)
{
	return rankingGetKthHighestCount(spaceSaving->ranking, k);
}

uintptr_t
spaceSavingGetCurSize(OMRSpaceSaving *spaceSaving)
{
	return spaceSaving->ranking->curSize;
}
