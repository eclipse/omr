/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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

#ifndef MM_OMRGCAPI_HPP_
#define MM_OMRGCAPI_HPP_

/*
 * @ddr_namespace: default
 */

#include "omr.h"
#include "objectdescription.h"
#include "omrcomp.h"
#include "j9nongenerated.h"

class MM_AllocateInitialization;

#ifdef __cplusplus
extern "C" {
#endif

/* Runtime API */

/* Caller is expected to initialize the allocation description (MM_AllocateInitialization::getAllocateDescription()) prior to call */
omrobjectptr_t OMR_GC_AllocateObject(OMR_VMThread * omrVMThread, MM_AllocateInitialization *allocator);

omr_error_t OMR_GC_SystemCollect(OMR_VMThread* omrVMThread, uint32_t gcCode);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* MM_OMRGCAPI_HPP_ */
