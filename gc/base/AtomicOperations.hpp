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
 ******************************************************************************/

#if !defined(ATOMIC_OPERATIONS_HPP_)
#define ATOMIC_OPERATIONS_HPP_

#include <stdlib.h>

#include "omrcfg.h"
#include "omrcomp.h"

#include "AtomicSupport.hpp"
#include "Math.hpp"
#include "omrutil.h"

/**
 * Provide atomic access to data store.
 */
class MM_AtomicOperations
{
public:

	/**
	 * If the CPU supports it, emit an instruction to yield the CPU to another thread.
	 */
	MMINLINE_DEBUG static void
	yieldCPU()
	{
		VM_AtomicSupport::yieldCPU();
	}

	/**
	 * Generates platform-specific machine code that performs no operation.
	 */
	MMINLINE_DEBUG static void
	nop()
	{
		VM_AtomicSupport::nop();
	}

	/**
	 * @Deprecated use the readWriteBarrier
	 */
	MMINLINE_DEBUG static void
	sync()
	{
		readWriteBarrier();
	}

	/**
	 * @Deprecated use the writeBarrier
	 */
	MMINLINE_DEBUG static void
	storeSync()
	{
		writeBarrier();
	}

	/**
	 * @Deprecated use the readBarrier
	 */
	MMINLINE_DEBUG static void
	loadSync()
	{
		readBarrier();
	}

	/**
	 * Creates a memory barrier.
	 * On a given processor, any load or store instructions ahead
	 * of the sync instruction in the program sequence must complete their accesses to memory
	 * first, and then any load or store instructions after sync can begin.
	 */
	MMINLINE_DEBUG static void
	readWriteBarrier()
	{
		VM_AtomicSupport::readWriteBarrier();
	}

	/**
	 * Creates a store barrier.
	 * Provides the same ordering function as the sync instruction, except that a load caused 
	 * by an instruction following the storeSync may be performed before a store caused by 
	 * an instruction that precedes the storeSync, and the ordering does not apply to accesses 
	 * to I/O memory (memory-mapped I/O).
	 */ 
	MMINLINE_DEBUG static void
	writeBarrier()
	{
		VM_AtomicSupport::writeBarrier();
	}

	/**
	 * Creates a load barrier.
	 * Causes the processor to discard any prefetched (and possibly speculatively executed) 
	 * instructions and refetch the next following instructions. It is used to ensure that 
	 * no loads following entry into a critical section can access data (because of aggressive 
	 * out-of-order and speculative execution in the processor) before the lock is acquired.
	 */
	MMINLINE_DEBUG static void
	readBarrier()
	{
		VM_AtomicSupport::readBarrier();
	}

	/**
	 * Store unsigned 32 bit value at memory location as an atomic operation.
	 * Compare the unsigned 32 bit value at memory location pointed to by <b>address</b>.  If it is
	 * equal to <b>oldValue</b> then update this memory location with <b>newValue</b>
	 * else retain the <b>oldValue</b>.
	 * 
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param oldValue The expected value at memory address
	 * @param newValue The new value to be stored at memory address
	 * 
	 * @return the value at memory location <b>address</b> BEFORE the store was attempted
	 */
	MMINLINE_DEBUG static uint32_t
	lockCompareExchangeU32(volatile uint32_t *address, uint32_t oldValue, uint32_t newValue)
	{
		return VM_AtomicSupport::lockCompareExchangeU32(address, oldValue, newValue);
	}

	/**
	 * Store value at memory location as an atomic operation.
	 * Compare the value at memory location pointed to by <b>address</b>.  If it is
	 * equal to <b>oldValue</b> then update this memory location with <b>newValue</b>
	 * else retain the <b>oldValue</b>.
	 * 
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param oldValue The expected value at memory address
	 * @param newValue The new value to be stored at memory address
	 * 
	 * @return the value at memory location <b>address</b> BEFORE the store was attempted
	 */
	MMINLINE_DEBUG static uintptr_t
	lockCompareExchange(volatile uintptr_t * address, uintptr_t oldValue, uintptr_t newValue)
	{
		return VM_AtomicSupport::lockCompareExchange(address, oldValue, newValue);
	}

	/**
	 * Store unsigned 64 bit value at memory location as an atomic operation.
	 * Compare the unsigned 64 bit value at memory location pointed to by <b>address</b>.  If it is
	 * equal to <b>oldValue</b> then update this memory location with <b>newValue</b>
	 * else retain the <b>oldValue</b>.
	 * 
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param oldValue The expected value at memory address
	 * @param newValue The new value to be stored at memory address
	 * 
	 * @return the value at memory location <b>address</b> BEFORE the store was attempted
	 */
	MMINLINE_DEBUG static uint64_t
	lockCompareExchangeU64(volatile uint64_t *address, uint64_t oldValue, uint64_t newValue)
	{
		return VM_AtomicSupport::lockCompareExchangeU64(address, oldValue, newValue);
	}

	/**
	 * Add a number to the value at a specific memory location as an atomic operation.
	 * Adds the value <b>addend</b> to the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param addend The value to be added
	 *
	 * @return The value at memory location <b>address</b> AFTER the add is completed
	 */
	MMINLINE_DEBUG static uintptr_t
	add(volatile uintptr_t *address, uintptr_t addend)
	{
		return VM_AtomicSupport::add(address, addend);
	}

	/**
	 * Add a 32 bit number to the value at a specific memory location as an atomic operation.
	 * Adds the value <b>addend</b> to the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param addend The value to be added
	 *
	 * @return The value at memory location <b>address</b>
	 */
	MMINLINE_DEBUG static uintptr_t
	addU32(volatile uint32_t *address, uint32_t addend)
	{
		return VM_AtomicSupport::addU32(address, addend);
	}

	/**
	 * Add a 64 bit number to the value at a specific memory location as an atomic operation.
	 * Adds the value <b>addend</b> to the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param addend The value to be added
	 *
	 * @return The value at memory location <b>address</b>
	 */
	MMINLINE_DEBUG static uint64_t
	addU64(volatile uint64_t *address, uint64_t addend)
	{
		return VM_AtomicSupport::addU64(address, addend);
	}

	/**
	 * Add double float to the value at a specific memory location as an atomic operation.
	 * Adds the value <b>addend</b> to the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param addend The value to be added
	 *
	 * @return The value at memory location <b>address</b>
	 */
	MMINLINE_DEBUG static double
	addDouble(volatile double *address, double addend)
	{
		return VM_AtomicSupport::addDouble(address, addend);
	}

	/**
	 * Subtracts a number from the value at a specific memory location as an atomic operation.
	 * Subtracts the value <b>value</b> from the value stored at memory location pointed
	 * to by <b>address</b>.
	 * 
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param value The value to be subtracted
	 * 
	 * @return The value at memory location <b>address</b>
	 */
	MMINLINE_DEBUG static uintptr_t
	subtract(volatile uintptr_t *address, uintptr_t value)
	{
		return VM_AtomicSupport::subtract(address, value);
	}
	
	/**
	 * Subtracts a 32 bit number from the value at a specific memory location as an atomic operation.
	 * Subtracts the value <b>value</b> from the value stored at memory location pointed
	 * to by <b>address</b>.
	 * 
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param value The value to be subtracted
	 * 
	 * @return The value at memory location <b>address</b>
	 */
	MMINLINE_DEBUG static uintptr_t
	subtractU32(volatile uint32_t *address, uint32_t value)
	{
		return VM_AtomicSupport::subtractU32(address, value);
	}

	/**
	 * Subtracts a 64 bit number from the value at a specific memory location as an atomic operation.
	 * Subtracts the value <b>value</b> from the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param value The value to be subtracted
	 *
	 * @return The value at memory location <b>address</b>
	 */
	MMINLINE_DEBUG static uint64_t
	subtractU64(volatile uint64_t *address, uint64_t value)
	{
		return VM_AtomicSupport::subtractU64(address, value);
	}

	/**
	 * Store value at memory location.
	 * Stores <b>value</b> at memory location pointed to be <b>address</b>.
	 * 
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param value The value to be stored
	 * 
	 * @note This method can spin indefinitely while attempting to write the new value.
	 */
	MMINLINE_DEBUG static void
	set(volatile uintptr_t *address, uintptr_t value)
	{
		VM_AtomicSupport::set(address, value);
	}

	/**
	 * Store 64-bit value at memory location.
	 * Stores <b>value</b> at memory location pointed to be <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param value The value to be stored
	 *
	 * @note This method can spin indefinitely while attempting to write the new value.
	 */
	MMINLINE_DEBUG static void
	setU64(volatile uint64_t *address, uint64_t value)
	{
		VM_AtomicSupport::setU64(address, value);
	}

	/**
	 * Load a 64-bit value from memory location.
	 * On a 32-bit platform, atomically read a 64-bit value using LCE. Just read and return value on 64-bit platforms.
	 *
	 * @param address The memory location to be read
	 *
	 * @return the value stored at the address.
	 */
	MMINLINE_DEBUG static uint64_t
	getU64(volatile uint64_t *address)
	{
		return VM_AtomicSupport::getU64(address);
	}

	/**
	 * Subtracts the given subtrahend from the value at a specific memory location, saturating to zero, as an atomic operation.
	 * Subtracts <b>subtrahend</b> from the value stored at memory location pointed to by <b>address</b>, saturing to zero.
	 * 
	 * @param address The memory location to be updated
	 * @param lockingWord The location used to lock the slot for exchange
	 * @param subtrahend The value to be subtracted
	 * 
	 * @return The value at memory location <b>address</b> AFTER the subtraction operation has been completed
	 */
	MMINLINE_DEBUG static uintptr_t
	saturatingSubtract(volatile uintptr_t *address, uintptr_t subtrahend)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uintptr_t *localAddr = address;
		
		uintptr_t oldValue = (uintptr_t)*localAddr;
		uintptr_t newValue = MM_Math::saturatingSubtract(oldValue, subtrahend);
		while ((lockCompareExchange(localAddr, oldValue, newValue)) != oldValue) {
			oldValue = (uintptr_t)*localAddr;
			newValue = MM_Math::saturatingSubtract(oldValue, subtrahend);
		}
		return newValue;
	}

};

#endif /* ATOMIC_OPERATIONS_HPP_ */
