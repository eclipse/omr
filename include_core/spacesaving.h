/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
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
#if !defined(SPACESAVING_H_)
#define SPACESAVING_H_
#include "ranking.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	OMRRanking *ranking;
	OMRPortLibrary *portLib;
} OMRSpaceSaving;

OMRSpaceSaving *spaceSavingNew(OMRPortLibrary *portLibrary, uint32_t size);
void spaceSavingFree(OMRSpaceSaving *spaceSaving);
void spaceSavingUpdate(OMRSpaceSaving *spaceSaving, void *data, uintptr_t count);
void spaceSavingClear(OMRSpaceSaving *spaceSaving);
void *spaceSavingGetKthMostFreq(OMRSpaceSaving *spaceSaving, uintptr_t k);
uintptr_t spaceSavingGetKthMostFreqCount(OMRSpaceSaving *spaceSaving, uintptr_t k);
uintptr_t spaceSavingGetCurSize(OMRSpaceSaving *spaceSaving);

#ifdef __cplusplus
}
#endif


#endif
