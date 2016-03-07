/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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

