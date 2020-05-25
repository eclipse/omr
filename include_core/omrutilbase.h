/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#ifndef OMRUTILBASE_H_INCLUDED
#define OMRUTILBASE_H_INCLUDED

#include <stdint.h>
#include "omrcfg.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/* ---------------- AtomicFunctions.cpp ---------------- */

/**
 * @brief Store value at memory location as an atomic operation.
 * Compare the value at memory location pointed to by location.  If it is
 * equal to oldValue, then update this memory location with newValue
 * else retain the oldValue.
 *
 * @param[in] location The memory location to be updated
 * @param[in] oldValue The expected value at memory address
 * @param[in] newValue The new value to be stored at memory address
 *
 * @return the value at memory location/address before the store was attempted
 */
uintptr_t
compareAndSwapUDATA(uintptr_t *location, uintptr_t oldValue, uintptr_t newValue);

/**
 * @brief Store unsigned 32 bit value at memory location as an atomic operation.
 * Compare the unsigned 32 bit value at the provided memory location.  If it is
 * equal to oldValue then update this memory location with newValue
 * else retain the oldValue.
 *
 * @param[in] location The memory location to be updated
 * @param[in] oldValue The expected value at memory address
 * @param[in] newValue The new value to be stored at memory address
 *
 * @return the value at memory location/address before the store was attempted
 */
uint32_t
compareAndSwapU32(uint32_t *location, uint32_t oldValue, uint32_t newValue);

/**
 * @brief Creates a load barrier. Causes the processor to discard any pre-fetched (and
 * possibly speculatively executed) instructions and re-fetch the next following instructions.
 * It is used to ensure that no loads following entry into a critical section can access
 * data (because of aggressive out-of-order and speculative execution in the processor) before
 * the lock is acquired.
 *
 * @param[in] void
 *
 * @return void
 */
void
issueReadBarrier(void);

/**
 * @brief Creates a memory barrier. On a given processor, any load or store instructions
 * ahead of the sync instruction in the program sequence must complete their accesses to
 * memory first, and then any load or store instructions after sync can begin.
 *
 * @param[in] void
 *
 * @return void
 */
void
issueReadWriteBarrier(void);

/**
 * @brief Creates a store barrier. Provides the same ordering function as the sync instruction,
 * except that a load caused by an instruction following the storeSync may be performed
 * before a store caused by an instruction that precedes the storeSync, and the ordering
 * does not apply to accesses to I/O memory (memory-mapped I/O).
 *
 * @param[in] void
 *
 * @return void
 */
void
issueWriteBarrier(void);

/**
 * @brief Add a number to the value at a specific memory location as an atomic operation.
 * Adds the addend to the value stored at memory location pointed to by address.
 *
 * @param[in] address The memory location to be updated
 * @param[in] addend The value to be added
 *
 * @return The value at memory location address after the add is completed
 */
uintptr_t
addAtomic(volatile uintptr_t *address, uintptr_t addend);

/**
 * @brief Subtracts a number from the value at a specific memory location as an atomic
 * operation. Subtracts the value from the value stored at memory location pointed to by
 * address.
 *
 * @param[in] address The memory location to be updated
 * @param[in] value The value to be subtracted
 *
 * @return The value at memory location address
 */
uintptr_t
subtractAtomic(volatile uintptr_t *address, uintptr_t value);

/**
 * @brief Store value at memory location. Stores value at memory
 * location pointed to be address.
 *
 * @param[in] address The memory location to be updated
 * @param[in] value The value to be stored
 *
 * @return The value at memory location address
 *
 * @note This method can spin indefinitely while attempting to write
 * the new value.
 */
uintptr_t setAtomic(volatile uintptr_t *address, uintptr_t value);

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
 *
 * @return  The old value read from addr
 */
uint64_t
J9CAS8Helper(volatile uint64_t *addr, uint32_t compareLo, uint32_t compareHi, uint32_t swapLo, uint32_t swapHi);

#endif /* !OMR_ENV_DATA64 && (AIXPPC || LINUXPPC) */


#if defined(__riscv)
/* ---------------- cas32helper.s ---------------- */
/**
 * @brief Perform a compare and swap of a 32-bit value.
 *
 * @param[in] addr          The address of the memory address
 * @param[in] compareValue  The value to be compared
 * @param[in] swapValue     The new value to be swapped to the specified memory address
 *
 * @return  The old value read from addr
 */
uint32_t
RiscvCAS32Helper(volatile uint32_t *addr, uint32_t compareValue, uint32_t swapValue);

/* ---------------- cas64helper.s ---------------- */
#if defined(OMR_ENV_DATA64)
/**
 * @brief Perform a compare and swap of a 64-bit value on a 64-bit system.
 *
 * @param[in] addr          The address of the memory address
 * @param[in] compareValue  The value to be compared
 * @param[in] swapValue     The new value to be swapped to the specified memory address
 *
 * @return  The old value read from addr
 */
uint64_t
RiscvCAS64Helper(volatile uint64_t *addr, uint64_t compareValue, uint64_t swapValue);
#endif /* OMR_ENV_DATA64 */
#endif /* __riscv */

/* ---------------- gettimebase.c ---------------- */

/**
 * @brief Return the time-base value stored in the Time Base register.
 *
 * @param[in] void
 *
 * @return the time-base value
 */
uint64_t
getTimebase(void);

/* ---------------- zbarrier.s ---------------- */
#if defined(J9ZOS390)
/**
 * @brief zOS Read-write barrier.
 *
 * @param[in] void
 *
 * @return void
 */
void
J9ZOSRWB(void);
#endif /* defined(J9ZOS390) */

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* OMRUTILBASE_H_INCLUDED */
