/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef algorithm_test_internal_h
#define algorithm_test_internal_h

/**
* This file contains implementation-private function prototypes and
* type definitions for the ALGORITHM_TEST module.
*
*/

#include "omrport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PoolInputData {
	const char *poolName;
	uint32_t structSize;
	uint32_t numberElements;
	uint32_t elementAlignment;
	uint32_t padding;
	uintptr_t poolFlags;
} PoolInputData;

typedef struct HashtableInputData {
	const char* hashtableName;
	const uintptr_t* data;
	uintptr_t dataLength;
	uint32_t listToTreeThreshold;
	BOOLEAN forceCollisions;
	BOOLEAN collisionResistant;
	uint16_t padding;
} HashtableInputData;

/* ---------------- avltest.c ---------------- */

/**
* @brief
* @param *portLib
* @param *testData
* @return int32_t
*/
int32_t
buildAndVerifyAVLTree(OMRPortLibrary *portLib, const char *success, const char *testData);

/* ---------------- pooltest.c ---------------- */

/**
* @brief
* @param *portLib
* @param *input
* @return int32_t
*/
int32_t
createAndVerifyPool(OMRPortLibrary *portLib, PoolInputData *input);

/**
* @brief
* @param *portLib
* @return int32_t
*/
int32_t
testPoolPuddleListSharing(OMRPortLibrary *portLib);

/* ---------------- hooktest.c ---------------- */

/**
* @brief
* @param *portLib
* @param *passCount
* @param *failCount
* @return int32_t
*/
int32_t
verifyHookable(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount);

/* ---------------- hashtabletest.c ---------------- */

/**
* @brief
* @param *portLib
* @param *passCount
* @param *failCount
* @return int32_t
*/
int32_t
verifyHashtable(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount);


int32_t
buildAndVerifyHashtable(OMRPortLibrary *portLib, HashtableInputData *inputData);

#ifdef __cplusplus
}
#endif

#endif /* algorithm_test_internal_h */
