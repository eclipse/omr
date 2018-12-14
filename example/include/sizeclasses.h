/*******************************************************************************
 * Copyright (c) 2006, 2016 IBM Corp. and others
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

#ifndef SIZECLASSES_H_
#define SIZECLASSES_H_

/*
 * @ddr_namespace: default
 */

#include "omrcomp.h"

#if defined(OMR_GC_SEGREGATED_HEAP)

/* The number of non-zero allocation sizes defined in the SMALL_SIZECLASSES array. */
#define OMR_SIZECLASSES_NUM_SMALL 0x0F

/* The index of the smallest non-zero small size class in the SMALL_SIZECLASSES array. */
#define OMR_SIZECLASSES_MIN_SMALL 0x1
/* The index of the largest small size class in the SMALL_SIZECLASSES array. */
#define OMR_SIZECLASSES_MAX_SMALL OMR_SIZECLASSES_NUM_SMALL
/* Reserved for allocation of arraylets */
#define OMR_SIZECLASSES_ARRAYLET (OMR_SIZECLASSES_NUM_SMALL + 1)
/* Reserved for allocation of large (>OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES) objects */
#define OMR_SIZECLASSES_LARGE (OMR_SIZECLASSES_NUM_SMALL + 2)

/* Logarithm of the smallest small class size */
#define OMR_SIZECLASSES_LOG_SMALLEST 0x04
/* Logarithm of the largest small class size */
#define OMR_SIZECLASSES_LOG_LARGEST 0x0B

/* Largest allocatable small object size in bytes */
#define OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES (1 << OMR_SIZECLASSES_LOG_LARGEST)

/* The initial number of size classes and their sizes.
 *
 * Because of alignment considerations, we must ensure there
 * are never two adjacent size classes both of which are not multiples of 8.
 * Note that this array must be of size OMR_SIZECLASSES_NUM_SMALL+1. Note that
 * the 0 size class isn't used since there are no 0-size objects.
 *
 * Trivial example below admits 15 allocation sizes up to 2k.
 */
#define SMALL_SIZECLASSES	{ 0, 16, 32, 64, 96, 160, 240, 352, 456, 592, 760, 968, 1200, 1520, 1760, 2048 }

typedef struct OMR_SizeClasses {
    uintptr_t smallCellSizes[OMR_SIZECLASSES_MAX_SMALL + 1];
    uintptr_t smallNumCells[OMR_SIZECLASSES_MAX_SMALL + 1];
    uintptr_t sizeClassIndex[OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES >> 2];
} OMR_SizeClasses;

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* SIZECLASSES_H_ */
