/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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
#if !defined(SPACESAVING_H_)
#define SPACESAVING_H_

/*
 * @ddr_namespace: default
 */

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
