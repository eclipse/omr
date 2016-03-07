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
#ifndef OMRUTILBASE_H_INCLUDED
#define OMRUTILBASE_H_INCLUDED

#include <stdint.h>
#include "omrcfg.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/* ---------------- AtomicFunctions.cpp ---------------- */
uintptr_t compareAndSwapUDATA(uintptr_t *location, uintptr_t oldValue, uintptr_t newValue, uintptr_t *spinlock);
uintptr_t compareAndSwapUDATANoSpinlock(uintptr_t *location, uintptr_t oldValue, uintptr_t newValue);
uint32_t compareAndSwapU32(uint32_t *location, uint32_t oldValue, uint32_t newValue, uintptr_t *spinlock);
uint32_t compareAndSwapU32NoSpinlock(uint32_t *location, uint32_t oldValue, uint32_t newValue);
void issueReadBarrier(void);
void issueReadWriteBarrier(void);
void issueWriteBarrier(void);

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
