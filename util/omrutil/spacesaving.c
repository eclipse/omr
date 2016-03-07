/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2010, 2015
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
