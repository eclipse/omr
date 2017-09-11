/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
#ifndef OMRUTILBASE_H_INCLUDED
#define OMRUTILBASE_H_INCLUDED

#include <stdint.h>
#include "omrcfg.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/* ---------------- AtomicFunctions.cpp ---------------- */
uintptr_t compareAndSwapUDATA(uintptr_t *location, uintptr_t oldValue, uintptr_t newValue);
uint32_t compareAndSwapU32(uint32_t *location, uint32_t oldValue, uint32_t newValue);
void issueReadBarrier(void);
void issueReadWriteBarrier(void);
void issueWriteBarrier(void);
uintptr_t addAtomic(volatile uintptr_t *address, uintptr_t addend);
uintptr_t subtractAtomic(volatile uintptr_t *address, uintptr_t value);

/* ---------------- cas8help.s ---------------- */
#if !defined(OMR_ENV_DATA64) && (defined(AIXPPC) || defined(LINUXPPC))

/**
 * @brief Perform a compare and swap of a 64-bit value on a 32-bit system.
 *
 * @param[in] addr  The address of the 8-aligned memory address
 * @param[in] compareLo  Low part of compare value
 * @param[in] compareHi  High part of compare value
 * @param[in] swapLo  Low  part of swap value
 * @param[in] swapHi  High part of swap value
 * @return  The old value read from addr
 */
uint64_t J9CAS8Helper(volatile uint64_t *addr, uint32_t compareLo, uint32_t compareHi, uint32_t swapLo, uint32_t swapHi);

#endif /* !OMR_ENV_DATA64 && (AIXPPC || LINUXPPC) */


/* ---------------- gettimebase.c ---------------- */
uint64_t getTimebase(void);

/* ---------------- zbarrier.s ---------------- */
#if defined(J9ZOS390)
void J9ZOSRWB(void);
#endif /* defined(J9ZOS390) */

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* OMRUTILBASE_H_INCLUDED */
