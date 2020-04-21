/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#if !defined(ATOMIC_SUPPORT_HPP_)
#define ATOMIC_SUPPORT_HPP_

/* Hide the compiler specific bits of this file from DDR since it parses with GCC. */
/* TODO : there should be a better #define for this! */
#if defined(TYPESTUBS_H)
#define ATOMIC_SUPPORT_STUB
#endif /* defined(TYPESTUBS_H) */

#if defined(OMRZTPF)
#include <tpf/cmpswp.h>
#endif

#include <stdlib.h>
#if !defined(ATOMIC_SUPPORT_STUB)
#if defined(J9ZOS39064)
/* need this for __csg() */
#include <builtins.h>
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif /* defined(_MSC_VER) */

#endif /* !defined(ATOMIC_SUPPORT_STUB) */

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrutilbase.h"

/*
 * Platform dependent synchronization functions.
 * Do not call directly, use the methods from VM_AtomicSupport.
 */
#if !defined(ATOMIC_SUPPORT_STUB)
#if defined(AIXPPC)
/* The default hardware priorities on AIX is MEDIUM(4).
 * For AIX, dropSMT should drop to LOW(2) and 
 * restoreSMT should raise back to MEDIUM(4)
 */
		inline void __dropSMT() { __asm__ __volatile__ ("or 1,1,1"); }
		inline void __restoreSMT() { __asm__ __volatile__ ("or 2,2,2"); }
#elif defined(LINUXPPC) /* defined(AIXPPC) */
/* The default hardware priorities on LINUXPPC is MEDIUM-LOW(3).
 * For LINUXPPC, dropSMT should drop to VERY-LOW(1) and 
 * restoreSMT should raise back to MEDIUM-LOW(3)
 */
		inline void __dropSMT() {  __asm__ __volatile__ ("or 31,31,31"); }
		inline void __restoreSMT() {  __asm__ __volatile__ ("or 6,6,6"); }

#elif defined(_MSC_VER) /* defined (LINUXPPC) */
		inline void __yield() { _mm_pause(); }
#elif defined(__GNUC__) && (defined(J9X86) || defined(J9HAMMER))
		inline void __yield() { __asm volatile ("pause"); }
#else
		inline void __yield() { __asm volatile ("# AtomicOperations::__yield"); }
#endif /* __GNUC__ && (J9X86 || J9HAMMER) */

#if defined(_MSC_VER)
		/* use compiler intrinsic */
#elif defined(LINUXPPC) || defined(AIXPPC)
		inline void __nop() { __asm__ __volatile__ ("nop"); }
		inline void __yield() { __asm__ __volatile__ ("or 27,27,27"); }
#elif defined(LINUX) && (defined(S390) || defined(S39064))
		/*
		 * nop instruction requires operand https://bugzilla.redhat.com/show_bug.cgi?id=506417
		 */
		inline void __nop() { __asm__ volatile ("nop 0"); }
#else /* GCC && XL */
		inline void __nop() { __asm__ volatile ("nop"); }
#endif

#if defined(AIXPPC) || defined(LINUXPPC)
#if defined(__GNUC__) && !defined(__clang__)
	/* GCC compiler does not provide the same intrinsics. */

	/**
	 * Synchronize
	 * Ensures that all instructions preceding the function the call to __sync complete before any instructions following
	 * the function call can execute.
	 */
	inline void __sync() { asm volatile ("sync"); }
	/**
	 * Instruction Synchronize
	 * Waits for all previous instructions to complete and then discards any prefetched instructions, causing subsequent
	 * instructions to be fetched (or refetched) and executed in the context established by previous instructions.
	 */
	inline void __isync() {	asm volatile ("isync");	}
	/**
	 * Load Word Synchronize
	 * Ensures that all store instructions preceding the call to __lwsync complete before any new instructions can be executed
	 * on the processor that executed the function. This allows you to synchronize between multiple processors with minimal
	 * performance impact, as __lwsync does not wait for confirmation from each processor.
	 */
	inline void __lwsync() { asm volatile ("lwsync"); }
#endif /* defined(__GNUC__) && !defined(__clang__) */
#endif /* AIXPPC || LINUXPPC */
#endif /* !defined(ATOMIC_SUPPORT_STUB) */

#if defined(OMR_ARCH_X86) || defined(OMR_ARCH_S390)
/* On x86 and s390 processors, allow the pre-read optimization in compare and swap.
 * Benchmarking has proven this to be a negative on Power and no benchmarking has been
 * performed on ARM.
 */
#define ATOMIC_ALLOW_PRE_READ
#endif /* defined(OMR_ARCH_X86) || defined(OMR_ARCH_S390) */

/**
 * Provide atomic access to data store.
 */
class VM_AtomicSupport
{
public:

	/**
	 * If the CPU supports it, emit an instruction to yield the CPU to another thread.
	 */
	VMINLINE static void
	yieldCPU()
	{
#if !defined(ATOMIC_SUPPORT_STUB)
		__yield();
#endif /* !defined(ATOMIC_SUPPORT_STUB) */
	}

	/**
	 * Generates platform-specific machine code that performs no operation.
	 */
	VMINLINE static void
	nop()
	{
#if !defined(ATOMIC_SUPPORT_STUB)
		__nop();
#endif /* !defined(ATOMIC_SUPPORT_STUB) */
	}

	/**
	 * Prevents compiler reordering of reads and writes across the barrier.
	 * This does not prevent processor reordering.
	 */
	VMINLINE static void
	compilerReorderingBarrier()
	{
#if !defined(ATOMIC_SUPPORT_STUB)
#if defined(__GNUC__)
		asm volatile("":::"memory");
#elif defined(_MSC_VER) /* __GNUC__ */
		_ReadWriteBarrier();
#elif defined(J9ZOS390) /* _MSC_VER */
		__fence();
#elif defined(__xlC__) /* J9ZOS390 */
		asm volatile("");
#else /* __xlC__ */
#error Unknown compiler
#endif /* __xlC__ */
#endif /* !defined(ATOMIC_SUPPORT_STUB) */
	}

	/**
	 * Creates a memory barrier.
	 * On a given processor, any load or store instructions ahead
	 * of the sync instruction in the program sequence must complete their accesses to memory
	 * first, and then any load or store instructions after sync can begin.
	 */
	VMINLINE static void
	readWriteBarrier()
	{
#if !defined(ATOMIC_SUPPORT_STUB)
#if defined(AIXPPC) || defined(LINUXPPC)
		__sync();
#elif defined(_MSC_VER)
		_ReadWriteBarrier();
#if defined(J9HAMMER)
		__faststorefence();
#else /* J9HAMMER */
		__asm { lock or dword ptr [esp],0 }
#endif /* J9HAMMER */
		_ReadWriteBarrier();
#elif defined(__GNUC__)
#if defined(J9X86)
		asm volatile("lock orl $0x0,(%%esp)" ::: "memory");
#elif defined(J9HAMMER)
		asm volatile("lock orl $0x0,(%%rsp)" ::: "memory");
#elif defined(ARM) /* defined(J9HAMMER) */
		__sync_synchronize();
#elif defined(OMR_ARCH_AARCH64) /* defined(ARM) */
		__asm __volatile ("dmb ish":::"memory");
#elif defined(S390) /* defined(OMR_ARCH_AARCH64) */
		asm volatile("bcr 15,0":::"memory");
#else /* defined(S390) */
		asm volatile("":::"memory");
#endif /* defined(J9X86) */
#elif defined(J9ZOS390)
		/* Replace this with an inline "bcr 15,0" whenever possible */
		__fence();
		J9ZOSRWB();
		__fence();
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
#endif /* !defined(ATOMIC_SUPPORT_STUB) */
	}

	/**
	 * Creates a store barrier.
	 * Provides the same ordering function as the sync instruction, except that a load caused
	 * by an instruction following the storeSync may be performed before a store caused by
	 * an instruction that precedes the storeSync, and the ordering does not apply to accesses
	 * to I/O memory (memory-mapped I/O).
	 */
	VMINLINE static void
	writeBarrier()
	{
	/* Neither x86 nor S390 require a write barrier - the compiler fence is sufficient */
#if !defined(ATOMIC_SUPPORT_STUB)
#if defined(AIXPPC) || defined(LINUXPPC)
		__lwsync();
#elif defined(_MSC_VER)
		_ReadWriteBarrier();
#elif defined(__GNUC__)
#if defined(ARM)
		__sync_synchronize();
#elif defined(OMR_ARCH_AARCH64) /* defined(ARM) */
		__asm __volatile ("dmb ishst":::"memory");
#else /* defined(OMR_ARCH_AARCH64) */
		asm volatile("":::"memory");
#endif /* defined(ARM) */
#elif defined(J9ZOS390)
		__fence();
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
#endif /* !defined(ATOMIC_SUPPORT_STUB) */
	}

	/**
	 * Creates a load barrier.
	 * Causes the processor to discard any prefetched (and possibly speculatively executed)
	 * instructions and refetch the next following instructions. It is used to ensure that
	 * no loads following entry into a critical section can access data (because of aggressive
	 * out-of-order and speculative execution in the processor) before the lock is acquired.
	 */
	VMINLINE static void
	readBarrier()
	{
	/* Neither x86 nor S390 require a read barrier - the compiler fence is sufficient */
#if !defined(ATOMIC_SUPPORT_STUB)
#if defined(AIXPPC) || defined(LINUXPPC)
		__isync();
#elif defined(_MSC_VER)
		_ReadWriteBarrier();
#elif defined(__GNUC__)
#if defined(ARM)
		__sync_synchronize();
#elif defined(OMR_ARCH_AARCH64) /* defined(ARM) */
		__asm __volatile ("dmb ishld":::"memory");
#else /* defined(OMR_ARCH_AARCH64) */
		asm volatile("":::"memory");
#endif /* defined(ARM) */
#elif defined(J9ZOS390)
		__fence();
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
#endif /* !defined(ATOMIC_SUPPORT_STUB) */
	}

	/**
	 * Creates a load barrier for use in monitor enter.
	 *
	 * Causes the processor to discard any prefetched (and possibly speculatively executed)
	 * instructions and refetch the next following instructions. It is used to ensure that
	 * no loads following entry into a critical section can access data (because of aggressive
	 * out-of-order and speculative execution in the processor) before the lock is acquired.
	 *
	 * This differs from readBarrier() in that it will break any memory transaction which is being
	 * used to avoid locking a monitor.
	 */
	VMINLINE static void
	monitorEnterBarrier()
	{
#if defined(AIXPPC) || defined(LINUXPPC)
		readWriteBarrier();
#else /* defined(AIXPPC) || defined(LINUXPPC) */
		readBarrier();
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
	}

	/**
	 * Store unsigned 32 bit value at memory location as an atomic operation.
	 * Compare the unsigned 32 bit value at memory location pointed to by <b>address</b>.  If it is
	 * equal to <b>oldValue</b> then update this memory location with <b>newValue</b>
	 * else retain the <b>oldValue</b>.
	 *
	 * @param address The memory location to be updated
	 * @param oldValue The expected value at memory address
	 * @param newValue The new value to be stored at memory address
	 * @param readBeforeCAS Controls whether a pre-read occurs before the CAS attempt (default false)
	 *
	 * @return the value at memory location <b>address</b> BEFORE the store was attempted
	 */
	VMINLINE static uint32_t
	lockCompareExchangeU32(volatile uint32_t *address, uint32_t oldValue, uint32_t newValue, bool readBeforeCAS = false)
	{
#if defined(ATOMIC_SUPPORT_STUB)
		return 0;
#else /* defined(ATOMIC_SUPPORT_STUB) */
#if defined(ATOMIC_ALLOW_PRE_READ)
		if (readBeforeCAS) {
			uint32_t currentValue = *address;
			if (currentValue != oldValue) {
				return currentValue;
			}
		}
#endif /* defined(ATOMIC_ALLOW_PRE_READ) */
#if defined(OMRZTPF)
		cs((cs_t *)&oldValue, (cs_t *)address, (cs_t)newValue);
		return oldValue;
#elif defined(__xlC__) /* defined(OMRZTPF) */
		__compare_and_swap((volatile int*)address, (int*)&oldValue, (int)newValue);
		return oldValue;
#elif defined(__GNUC__)  /* defined(__xlC__) */
		/* Assume GCC >= 4.2 */
		return __sync_val_compare_and_swap(address, oldValue, newValue);
#elif defined(_MSC_VER) /* defined(__GNUC__) */
		return (uint32_t)_InterlockedCompareExchange((volatile long *)address, (long)newValue, (long)oldValue);
#elif defined(J9ZOS390) /* defined(_MSC_VER) */
		/* V1.R13 has a compiler bug and if you pass a constant as oldValue it will cause c-stack corruption */
		volatile uint32_t old = oldValue;
		/* 390 cs() function defined in <stdlib.h>, doesn't expand properly to __cs1() which correctly deals with aliasing */
		__cs1((uint32_t *)&old, (uint32_t *)address, (uint32_t *)&newValue);
		return old;
#else /* defined(J9ZOS390) */
#error "lockCompareExchangeU32(): unsupported platform!"
#endif /* defined(__xlC__) */
#endif /* defined(ATOMIC_SUPPORT_STUB) */
	}

	/**
	 * Store unsigned 64 bit value at memory location as an atomic operation.
	 * Compare the unsigned 64 bit value at memory location pointed to by <b>address</b>.  If it is
	 * equal to <b>oldValue</b> then update this memory location with <b>newValue</b>
	 * else retain the <b>oldValue</b>.
	 *
	 * @param address The memory location to be updated
	 * @param oldValue The expected value at memory address
	 * @param newValue The new value to be stored at memory address
	 * @param readBeforeCAS Controls whether a pre-read occurs before the CAS attempt (default false)
	 *
	 * @return the value at memory location <b>address</b> BEFORE the store was attempted
	 */
	VMINLINE static uint64_t
	lockCompareExchangeU64(volatile uint64_t *address, uint64_t oldValue, uint64_t newValue, bool readBeforeCAS = false)
	{
#if defined(ATOMIC_SUPPORT_STUB)
		return 0;
#else /* defined(ATOMIC_SUPPORT_STUB) */
		/* For 64-bit CAS on 32-bit architectures, the pre-read is not
		 * reliable, as the value will not be read atomically.  As this
		 * is strictly a performance optimization, just don't do it in
		 * this case.
		 */
#if defined(ATOMIC_ALLOW_PRE_READ) && defined(OMR_ENV_DATA64)
		if (readBeforeCAS) {
			uint64_t currentValue = *address;
			if (currentValue != oldValue) {
				return currentValue;
			}
		}
#endif /* defined(ATOMIC_ALLOW_PRE_READ) */
#if defined(OMR_ARCH_POWER) && !defined(OMR_ENV_DATA64) /* defined(ATOMIC_SUPPORT_STUB) */
		return J9CAS8Helper(address, ((uint32_t*)&oldValue)[1], ((uint32_t*)&oldValue)[0], ((uint32_t*)&newValue)[1], ((uint32_t*)&newValue)[0]);
#elif defined(OMRZTPF) /* defined(OMR_ARCH_POWER) && !defined(OMR_ENV_DATA64) */
		csg((csg_t *)&oldValue, (csg_t *)address, (csg_t)newValue);
		return oldValue;
#elif defined(__xlC__) /* defined(OMRZTPF) */
		__compare_and_swaplp((volatile long*)address, (long*)&oldValue, (long)newValue);
		return oldValue;
#elif defined(__GNUC__) /* defined(__xlC__) */
		/* Assume GCC >= 4.2 */
		return __sync_val_compare_and_swap(address, oldValue, newValue);
#elif defined(_MSC_VER) /* defined(__GNUC__) */
		return (uint64_t)_InterlockedCompareExchange64((volatile __int64 *)address, (__int64)newValue, (__int64)oldValue);
#elif defined(J9ZOS390) /* defined(_MSC_VER) */
		 /* V1.R13 has a compiler bug and if you pass a constant as oldValue it will cause c-stack corruption */
		 volatile uint64_t old = oldValue;
#if defined(OMR_ENV_DATA64)
		 /* Call __csg directly as csg() does not exist */
		__csg((void*)&old, (void*)address, (void*)&newValue);
		return old;
#else /* defined(OMR_ENV_DATA64) */
		/* __cds1 does not write the swap value correctly, cds does the correct thing */
		cds((cds_t*)&old, (cds_t*)address, *(cds_t*)&newValue);
		return old;
#endif /* defined(OMR_ENV_DATA64) */
#else /* defined(J9ZOS390) */
#error "lockCompareExchangeU64(): unsupported platform!"
#endif /* defined(__xlC__) */
#endif /* defined(ATOMIC_SUPPORT_STUB) */
	}

	/**
	 * Store value at memory location as an atomic operation.
	 * Compare the value at memory location pointed to by <b>address</b>.  If it is
	 * equal to <b>oldValue</b> then update this memory location with <b>newValue</b>
	 * else retain the <b>oldValue</b>.
	 *
	 * @param address The memory location to be updated
	 * @param oldValue The expected value at memory address
	 * @param newValue The new value to be stored at memory address
	 * @param readBeforeCAS Controls whether a pre-read occurs before the CAS attempt (default false)
	 *
	 * @return the value at memory location <b>address</b> BEFORE the store was attempted
	 */
	VMINLINE static uintptr_t
	lockCompareExchange(volatile uintptr_t * address, uintptr_t oldValue, uintptr_t newValue, bool readBeforeCAS = false)
	{
#if defined(OMR_ENV_DATA64)
		return (uintptr_t)lockCompareExchangeU64((volatile uint64_t *)address, (uint64_t)oldValue, (uint64_t)newValue, readBeforeCAS);
#else /* defined(OMR_ENV_DATA64) */
		return (uintptr_t)lockCompareExchangeU32((volatile uint32_t *)address, (uint32_t)oldValue, (uint32_t)newValue, readBeforeCAS);
#endif /* defined(OMR_ENV_DATA64) */
	}

#if defined(OMR_ENV_DATA64)
	/**
	 * Store the unsigned 64-bit value at the memory location as an atomic operation, and
	 * return the old 64-bit value stored at the memory location.
	 *
	 * @param address The memory location to be updated
	 * @param newValue The new value to be stored at memory address
	 *
	 * @return the value at memory location <b>address</b> BEFORE the store was attempted
	 */
	VMINLINE static uint64_t
	lockExchangeU64(volatile uint64_t *address, uint64_t newValue)
	{
#if defined(ATOMIC_SUPPORT_STUB)
		return 0;
#elif defined(J9ZOS390) /* defined(ATOMIC_SUPPORT_STUB) */
		/* On zLinux, the fetch-and-store operation generates the same s390x assembly as the
		 * compare-and-swap operation. On zOS, the XLC equivalent of the fetch-and-store
		 * operation (__fetch_and_swap) does not compile. So, the lockExchange function is
		 * supported using the atomic set function on zOS, which is a wrapper for the
		 * lockCompareExchange function.
		 */
		return setU64(address, newValue);
#else /* defined(ATOMIC_SUPPORT_STUB) */
		readWriteBarrier();
#if defined(__GNUC__)
		return (uint64_t)__sync_lock_test_and_set(address, newValue);
#elif defined(__xlC__) /* defined(__GNUC__) */
#if ((__xlC__ > 0x0d01) || ((__xlC__ == 0x0d01) && (__xlC_ver__ >= 0x00000300))) /* XLC >= 13.1.3 */
		return (uint64_t)__fetch_and_swaplp((volatile unsigned long *)address, (unsigned long)newValue);
#else /* ((__xlC__ > 0x0d01) || ((__xlC__ == 0x0d01) && (__xlC_ver__ >= 0x00000300))) */
		return (uint64_t)__fetch_and_swaplp((volatile long *)address, (long)newValue);
#endif /* ((__xlC__ > 0x0d01) || ((__xlC__ == 0x0d01) && (__xlC_ver__ >= 0x00000300))) */
#elif defined(_MSC_VER) /* defined(__GNUC__) */
		return (uint64_t)_InterlockedExchange64((volatile __int64 *)address, (__int64)newValue);
#else /* defined(__GNUC__) */
#error "lockExchangeU64(): unsupported platform!"
#endif /* defined(__GNUC__) */
#endif /* defined(ATOMIC_SUPPORT_STUB) */
	}
#else /* defined(OMR_ENV_DATA64) */
	/**
	 * Store the unsigned 32-bit value at the memory location as an atomic operation, and
	 * return the old 32-bit value stored at the memory location.
	 *
	 * @param address The memory location to be updated
	 * @param newValue The new value to be stored at memory address
	 *
	 * @return the value at memory location <b>address</b> BEFORE the store was attempted
	 */
	VMINLINE static uint32_t
	lockExchangeU32(volatile uint32_t *address, uint32_t newValue)
	{
#if defined(ATOMIC_SUPPORT_STUB)
		return 0;
#elif defined(J9ZOS390) /* defined(ATOMIC_SUPPORT_STUB) */
		/* On zLinux, the fetch-and-store operation generates the same s390x assembly as the
		 * compare-and-swap operation. On zOS, the XLC equivalent of the fetch-and-store
		 * operation (__fetch_and_swap) does not compile. So, the lockExchange function is
		 * supported using the atomic set function on zOS, which is a wrapper for the
		 * lockCompareExchange function.
		 */
		return (uint32_t)set((volatile uintptr_t *)address, (uintptr_t)newValue);
#else /* defined(ATOMIC_SUPPORT_STUB) */
		readWriteBarrier();
#if defined(__GNUC__)
		return (uint32_t)__sync_lock_test_and_set(address, newValue);
#elif defined(__xlC__) /* defined(__GNUC__) */
#if ((__xlC__ > 0x0d01) || ((__xlC__ == 0x0d01) && (__xlC_ver__ >= 0x00000300))) /* XLC >= 13.1.3 */
		return (uint32_t)__fetch_and_swap((volatile unsigned int *)address, (unsigned int)newValue);
#else /* ((__xlC__ > 0x0d01) || ((__xlC__ == 0x0d01) && (__xlC_ver__ >= 0x00000300))) */
		return (uint32_t)__fetch_and_swap((volatile int *)address, (int)newValue);
#endif /* ((__xlC__ > 0x0d01) || ((__xlC__ == 0x0d01) && (__xlC_ver__ >= 0x00000300))) */
#elif defined(_MSC_VER) /* defined(__GNUC__) */
		return (uint32_t)_InterlockedExchange((volatile long *)address, (long)newValue);
#else /* defined(__GNUC__) */
#error "lockExchangeU32(): unsupported platform!"
#endif /* defined(__GNUC__) */
#endif /* defined(ATOMIC_SUPPORT_STUB) */
	}
#endif /* defined(OMR_ENV_DATA64) */

	/**
	 * Store the value at the memory location as an atomic operation, and return the old
	 * value stored at the memory location.
	 *
	 * @param address The memory location to be updated
	 * @param newValue The new value to be stored at memory address
	 *
	 * @return the value at memory location <b>address</b> BEFORE the store was attempted
	 */
	VMINLINE static uintptr_t
	lockExchange(volatile uintptr_t * address, uintptr_t newValue)
	{
#if defined(OMR_ENV_DATA64)
		return (uintptr_t)lockExchangeU64((volatile uint64_t *)address, (uint64_t)newValue);
#else /* defined(OMR_ENV_DATA64) */
		return (uintptr_t)lockExchangeU32((volatile uint32_t *)address, (uint32_t)newValue);
#endif /* defined(OMR_ENV_DATA64) */
	}

	/**
	 * Add a number to the value at a specific memory location as an atomic operation.
	 * Adds the value <b>addend</b> to the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param addend The value to be added
	 *
	 * @return The value at memory location <b>address</b> AFTER the add is completed
	 */
	VMINLINE static uintptr_t
	add(volatile uintptr_t *address, uintptr_t addend)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uintptr_t *localAddr = address;
		uintptr_t oldValue;

		oldValue = (uintptr_t)*localAddr;
		while ((lockCompareExchange(localAddr, oldValue, oldValue + addend)) != oldValue) {
			oldValue = (uintptr_t)*localAddr;
		}
		return oldValue + addend;
	}

	/**
	 * AND a mask with the value at a specific memory location as an atomic operation.
	 * ANDs the value <b>mask</b> with the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param mask The value to be added
	 *
	 * @return The value at memory location <b>address</b> BEFORE the AND is completed
	 */
	VMINLINE static uintptr_t
	bitAnd(volatile uintptr_t *address, uintptr_t mask)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uintptr_t *localAddr = address;
		uintptr_t oldValue;

		oldValue = (uintptr_t)*localAddr;
		while ((lockCompareExchange(localAddr, oldValue, oldValue & mask)) != oldValue) {
			oldValue = (uintptr_t)*localAddr;
		}
		return oldValue;
	}

	/**
	 * AND a mask with the value at a specific memory location as an atomic operation.
	 * ANDs the value <b>mask</b> with the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param mask The value to be added
	 *
	 * @return The value at memory location <b>address</b> BEFORE the AND is completed
	 */
	VMINLINE static uint32_t
	bitAndU32(volatile uint32_t *address, uint32_t mask)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uint32_t *localAddr = address;
		uint32_t oldValue;

		oldValue = (uint32_t)*localAddr;
		while ((lockCompareExchangeU32(localAddr, oldValue, oldValue & mask)) != oldValue) {
			oldValue = (uint32_t)*localAddr;
		}
		return oldValue;
	}

	/**
	 * OR a mask with the value at a specific memory location as an atomic operation.
	 * ORs the value <b>mask</b> with the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param mask The value to be added
	 *
	 * @return The value at memory location <b>address</b> BEFORE the OR is completed
	 */
	VMINLINE static uintptr_t
	bitOr(volatile uintptr_t *address, uintptr_t mask)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uintptr_t *localAddr = address;
		uintptr_t oldValue;

		oldValue = (uintptr_t)*localAddr;
		while ((lockCompareExchange(localAddr, oldValue, oldValue | mask)) != oldValue) {
			oldValue = (uintptr_t)*localAddr;
		}
		return oldValue;
	}

	/**
	 * OR a mask with the value at a specific memory location as an atomic operation.
	 * ORs the value <b>mask</b> with the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param mask The value to be added
	 *
	 * @return The value at memory location <b>address</b> BEFORE the OR is completed
	 */
	VMINLINE static uint32_t
	bitOrU32(volatile uint32_t *address, uint32_t mask)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uint32_t *localAddr = address;
		uint32_t oldValue;

		oldValue = (uint32_t)*localAddr;
		while ((lockCompareExchangeU32(localAddr, oldValue, oldValue | mask)) != oldValue) {
			oldValue = (uint32_t)*localAddr;
		}
		return oldValue;
	}

	/**
	 * Add a 32 bit number to the value at a specific memory location as an atomic operation.
	 * Adds the value <b>addend</b> to the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param addend The value to be added
	 *
	 * @return The value at memory location <b>address</b>
	 */
	VMINLINE static uintptr_t
	addU32(volatile uint32_t *address, uint32_t addend)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uint32_t *localAddr = address;
		uint32_t oldValue;

		oldValue = (uint32_t)*localAddr;
		while ((lockCompareExchangeU32(localAddr, oldValue, oldValue + addend)) != oldValue) {
			oldValue = (uint32_t)*localAddr;
		}
		return oldValue + addend;
	}

	/**
	 * Add a 64 bit number to the value at a specific memory location as an atomic operation.
	 * Adds the value <b>addend</b> to the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param addend The value to be added
	 *
	 * @return The value at memory location <b>address</b>
	 */
	VMINLINE static uint64_t
	addU64(volatile uint64_t *address, uint64_t addend)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uint64_t *localAddr = address;
		uint64_t oldValue;

		oldValue = (uint64_t)*localAddr;
		while ((lockCompareExchangeU64(localAddr, oldValue, oldValue + addend)) != oldValue) {
			oldValue = (uint64_t)*localAddr;
		}
		return oldValue + addend;
	}

	union DoubleConversionData {
		double asDouble;
		uint64_t asBits;
	};

	/**
	 * Add double float to the value at a specific memory location as an atomic operation.
	 * Adds the value <b>addend</b> to the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param addend The value to be added
	 *
	 * @return The value at memory location <b>address</b>
	 */
	VMINLINE static double
	addDouble(volatile double *address, double addend)
	{
		/* while casting address to a DoubleConversionData* will silence GCC's strict aliasing warnings, the
		 * code below is still dereferencing a type-punned pointer, and is thus undefined behaviour.
		 */

		/* Stop compiler optimizing away load of oldValue. data is a type-punned pointer. */
		volatile DoubleConversionData* data = (volatile DoubleConversionData*)address;

		DoubleConversionData oldData;
		DoubleConversionData newData;

		oldData.asDouble = data->asDouble;
		newData.asDouble = oldData.asDouble + addend;

		while (lockCompareExchangeU64(&data->asBits, oldData.asBits, newData.asBits) != oldData.asBits) {
			oldData.asDouble = data->asDouble;
			newData.asDouble = oldData.asDouble + addend;
		}
		return newData.asDouble;
	}

	/**
	 * Subtracts a number from the value at a specific memory location as an atomic operation.
	 * Subtracts the value <b>value</b> from the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param value The value to be subtracted
	 *
	 * @return The value at memory location <b>address</b>
	 */
	VMINLINE static uintptr_t
	subtract(volatile uintptr_t *address, uintptr_t value)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uintptr_t *localAddr = address;
		uintptr_t oldValue;

		oldValue = (uintptr_t)*localAddr;
		while ((lockCompareExchange(localAddr, oldValue, oldValue - value)) != oldValue) {
			oldValue = (uintptr_t)*localAddr;
		}
		return oldValue - value;
	}

	/**
	 * Subtracts a 64-bit number from the value at a specific memory location asn an atomic operation.
	 * Subtracts the value <b>value</b> from the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param value The value to be subtracted
	 *
	 * @return The value at memory location <b>address</b>
	 */
	VMINLINE static uint64_t
	subtractU64(volatile uint64_t *address, uint64_t value)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uint64_t *localAddr = address;
		uint64_t oldValue;

		oldValue = (uint64_t)*localAddr;
		while ((lockCompareExchangeU64(localAddr, oldValue, oldValue - value)) != oldValue) {
			oldValue = (uint64_t)*localAddr;
		}
		return oldValue - value;
	}

	/**
	 * Subtracts a 32 bit number from the value at a specific memory location asn an atomic operation.
	 * Subtracts the value <b>value</b> from the value stored at memory location pointed
	 * to by <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param value The value to be subtracted
	 *
	 * @return The value at memory location <b>address</b>
	 */
	VMINLINE static uintptr_t
	subtractU32(volatile uint32_t *address, uint32_t value)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uint32_t *localAddr = address;
		uint32_t oldValue;

		oldValue = (uint32_t)*localAddr;
		while ((lockCompareExchangeU32(localAddr, oldValue, oldValue - value)) != oldValue) {
			oldValue = (uint32_t)*localAddr;
		}
		return oldValue - value;
	}

	/**
	 * Store value at memory location.
	 * Stores <b>value</b> at memory location pointed to be <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param value The value to be stored
	 *
	 * @return The value at memory location <b>address</b>
	 *
	 * @note This method can spin indefinitely while attempting to write the new value.
	 */
	VMINLINE static uintptr_t
	set(volatile uintptr_t *address, uintptr_t value)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uintptr_t *localAddr = address;
		uintptr_t oldValue;

		oldValue = (uintptr_t)*localAddr;
		while ((lockCompareExchange(localAddr, oldValue, value)) != oldValue) {
			oldValue = (uintptr_t)*localAddr;
		}
		return oldValue;
	}

	/**
	 * Store 64-bit value at memory location.
	 * Stores <b>value</b> at memory location pointed to be <b>address</b>.
	 *
	 * @param address The memory location to be updated
	 * @param value The value to be stored
	 *
	 * @return The value at memory location <b>address</b>
	 *
	 * @note This method can spin indefinitely while attempting to write the new value.
	 */
	VMINLINE static uint64_t
	setU64(volatile uint64_t *address, uint64_t value)
	{
		/* Stop compiler optimizing away load of oldValue */
		volatile uint64_t *localAddr = address;
		uint64_t oldValue;

		oldValue = (uint64_t)*localAddr;
		while ((lockCompareExchangeU64(localAddr, oldValue, value)) != oldValue) {
			oldValue = (uint64_t)*localAddr;
		}
		return oldValue;
	}

	/**
	 * Load a 64-bit value from memory location.
	 * On a 32-bit platform, atomically read a 64-bit value using LCE. Just read and return value on 64-bit platforms.
	 *
	 * @param address The memory location to be read
	 *
	 * @return the value stored at the address.
	 */
	VMINLINE static uint64_t
	getU64(volatile uint64_t *address)
	{
		uint64_t value = *address;

#if !defined(OMR_ENV_DATA64)
		/* this is necessary to ensure atomic read of 64-bit value on 32/31-bit platforms */
		value = lockCompareExchangeU64(address, value, value);
#endif /* !defined(OMR_ENV_DATA64) */

		return value;
	}

	/**
	 * On PPC processors, lower the SMT thread priority.
	 */
	VMINLINE static void
	dropSMTThreadPriority()
	{
#if !defined(ATOMIC_SUPPORT_STUB)
#if defined(AIXPPC) || defined(LINUXPPC)
		__dropSMT();
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
#endif /* !defined(ATOMIC_SUPPORT_STUB) */
	}

	/**
	 * On PPC processors, restore the SMT thread priority.
	 */
	VMINLINE static void
	restoreSMTThreadPriority()
	{
#if !defined(ATOMIC_SUPPORT_STUB)
#if defined(AIXPPC) || defined(LINUXPPC)
		__restoreSMT();
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
#endif /* !defined(ATOMIC_SUPPORT_STUB) */
	}

	/**
	 * Return true approximately one time in frequency, which must be a power of 2.
	 * Processors which do not support timestamp reading always return true.
	 *
	 * @param[in] frequency
	 *
	 * @return see above
	 */
	VMINLINE static bool
	sampleTimestamp(uintptr_t frequency)
	{
		/* TODO: JAZZ103 51150 */
		return true;
	}

};

#endif /* ATOMIC_SUPPORT_HPP_ */
