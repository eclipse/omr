/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef algorithm_test_internal_h
#define algorithm_test_internal_h

/**
* @file algorithm_test_internal.h
* @brief Internal prototypes used within the ALGORITHM_TEST module.
*
* This file contains implementation-private function prototypes and
* type definitions for the ALGORITHM_TEST module.
*
*/

#include "omrport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- avltest.c ---------------- */

/**
* @brief
* @param *portLib
* @param *testListFile
* @param *passCount
* @param *failCount
* @return int32_t
*/
int32_t
verifyAVLTree(OMRPortLibrary *portLib, char *testListFile, uintptr_t *passCount, uintptr_t *failCount);

/* ---------------- pooltest.c ---------------- */

/**
* @brief
* @param *portLib
* @param *passCount
* @param *failCount
* @return int32_t
*/
int32_t
verifyPools(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount);

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

#ifdef __cplusplus
}
#endif

#endif /* algorithm_test_internal_h */
