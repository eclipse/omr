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

#ifndef hookable_api_h
#define hookable_api_h

/**
* @file hookable_api.h
* @brief Public API for the HOOKABLE module.
*
* This file contains public function prototypes and
* type definitions for the HOOKABLE module.
*
*/

#include "omrcomp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- hookable.c ---------------- */

struct OMRPortLibrary;
struct J9HookInterface;
/**
* @brief
* @param hookInterface
* @param portLib
* @param interfaceSize
* @return intptr_t
*/
intptr_t
J9HookInitializeInterface(struct J9HookInterface **hookInterface, OMRPortLibrary *portLib, size_t interfaceSize);

#ifdef __cplusplus
}
#endif

#endif /* hookable_api_h */

