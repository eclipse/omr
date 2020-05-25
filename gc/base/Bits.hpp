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

#if !defined(BITS_HPP_)
#define BITS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"

#if defined(OMR_ENV_DATA64)
#define J9BITS_BITS_IN_SLOT 64
#else
#define J9BITS_BITS_IN_SLOT 32
#endif /* defined(OMR_ENV_DATA64) */

class MM_Bits {
public:
	static MMINLINE uintptr_t convertBytesToSlots(uintptr_t x) {
		return x >> OMR_LOG_POINTER_SIZE;
	}

	static MMINLINE uintptr_t convertSlotsToBytes(uintptr_t x) {
		return x << OMR_LOG_POINTER_SIZE;
	}

	/**
	 * Checks if the uintptr_t's last bit is set or not.
	 * @param[in] object The uintptr_t to check
	 * @return true if the last bit is set, false otherwise.
	 */
	static MMINLINE bool isLowTagged(uintptr_t object) {
		return (object & 1) == 1;
	}

	/**
	 * Returns a uintptr_t with the last 2 bits <i>unset</i>.
	 * @param[in] object The uintptr_t whose bits you want to unset.
	 * @return The same uintptr_t, with the last 2 bits unset.
	 */
	static MMINLINE uintptr_t untag(uintptr_t object) {
		return object & ~((uintptr_t)3);
	}

	/**
	 * Calculate the number of bits set to 1 in the input word.
	 * @return Number of bits set to 1.
	 */
	MMINLINE static uintptr_t populationCount(uintptr_t input)
	{
		uintptr_t work, temp;

		work = input;
		if(0 == work) {
			return 0;
		}

#if !defined(OMR_ENV_DATA64)
		work = work - ((work >> 1) & ((uintptr_t)0x55555555));
		temp = ((work >> 2) & ((uintptr_t)0x33333333));
		work = (work & ((uintptr_t)0x33333333)) + temp;
		work = (work + (work >> 4)) & ((uintptr_t)0x0F0F0F0F);
		work = work + (work << 8);
		work = work + (work << 16);
		return work >> 24;
#else
		work = work - ((work >> 1) & ((uintptr_t)0x5555555555555555));
		temp = ((work >> 2) & ((uintptr_t)0x3333333333333333));
		work = (work & ((uintptr_t)0x3333333333333333)) + temp;
		work = (work + (work >> 4)) & ((uintptr_t)0x0F0F0F0F0F0F0F0F);
		work = work + (work << 8);
		work = work + (work << 16);
		work = work + (work << 32);
		return work >> 56;
#endif /* !defined(OMR_ENV_DATA64) */
	}

#if defined(OMR_OS_WINDOWS) && !defined(OMR_ENV_DATA64)
	/**
	 * Return the number of bits set to 0 before the first bit set to one starting at the lowest
	 * significant bit.
	 * @note If the input is 0, the result is undefined.
	 * @return Number of non-zero bits starting at the lowest significant bit.
	 */
#if defined(OMR_OS_WINDOWS)
	/* Implicit return in eax, not seen by compiler.  Disable compile warning C4035: no return value */
#pragma warning(disable:4035)
	MMINLINE static uintptr_t leadingZeroes(uintptr_t input)
	{
		_asm {
			bsf eax, input
		}
	}
#pragma warning(default:4035) /* re-enable warning */
#endif /* defined(OMR_OS_WINDOWS) */

	/**
	 * Return the number of bits set to 0 before the first bit set to one starting at the highest
	 * significant bit.
	 * @note If the input is 0, the result is undefined.
	 * @return Number of non-zero bits starting at the highest significant bit.
	 */
#if defined(OMR_OS_WINDOWS)
	/* Implicit return in eax, not seen by compiler.  Disable compile warning C4035: no return value */
#pragma warning(disable:4035)
	MMINLINE static uintptr_t trailingZeroes(uintptr_t input)
	{
		_asm {
			bsr eax, input
			neg eax
			add eax, J9BITS_BITS_IN_SLOT - 1
		}
	}
#pragma warning(default:4035) /* re-enable warning */
#endif /* defined(OMR_OS_WINDOWS) */

#elif defined(LINUX) && defined(J9HAMMER)
	/**
	 * Return the number of bits set to 0 before the first bit set to one starting at the lowest
	 * significant bit.
	 * @note If the input is 0, the result is undefined.
	 * @return Number of non-zero bits starting at the lowest significant bit.
	 */
	MMINLINE static uintptr_t leadingZeroes(uintptr_t input)
	{
		uintptr_t result;
		asm volatile(" bsfq %1, %0": "=r"(result): "rm"(input) );
		return result;
	}

	/**
	 * Return the number of bits set to 0 before the first bit set to one starting at the highest
	 * significant bit.
	 * @note If the input is 0, the result is undefined.
	 * @return Number of non-zero bits starting at the highest significant bit.
	 */
	MMINLINE static uintptr_t trailingZeroes(uintptr_t input)
	{
		uintptr_t result;
		asm volatile(
				" bsrq %1, %0;"
				" neg %0;"
				" add %2, %0;"
				: "=r"(result)
				: "rm"(input), "g"(J9BITS_BITS_IN_SLOT - 1) );
		return result;
	}

#else /* defined(LINUX) && defined(J9HAMMER) */


	/**
	 * Return the number of bits set to 0 before the first bit set to one starting at the lowest
	 * significant bit.
	 * @note If the input is 0, the result is undefined.
	 * @return Number of non-zero bits starting at the lowest significant bit.
	 */
	MMINLINE static uintptr_t leadingZeroes(uintptr_t input)
	{
		return populationCount(~(input | (0 - input)));
	}

	/**
	 * Return the number of bits set to 0 before the first bit set to one starting at the highest
	 * significant bit.
	 * Use a binary search style divide and conquer algorithm.  The check constantly splits the group
	 * in half, and reduces the working set to either the upper or lower half of the remaining value,
	 * depending on whether bits still exist in the upper half or not.
	 * @note If the input is 0, the result is undefined.
	 * @return Number of non-zero bits starting at the highest significant bit.
	 */
	MMINLINE static uintptr_t trailingZeroes(uintptr_t input)
	{
		uintptr_t work, carry, result;
		work = input;
		result = 0;

#if defined(OMR_ENV_DATA64)
		carry = (work < (((uintptr_t)1) << 32)) ? 0 : 32;
		result += carry;
		work = work >> carry;
#endif /* defined(OMR_ENV_DATA64) */

		carry = (work < (((uintptr_t)1) << 16)) ? 0 : 16;
		result += carry;
		work = work >> carry;

		carry = (work < (((uintptr_t)1) << 8)) ? 0 : 8;
		result += carry;
		work = work >> carry;

		carry = (work < (((uintptr_t)1) << 4)) ? 0 : 4;
		result += carry;
		work = work >> carry;

		carry = (work < (((uintptr_t)1) << 2)) ? 0 : 2;
		result += carry;
		work = work >> carry;

		result += (work >> 1);

		return J9BITS_BITS_IN_SLOT - 1 - result;
	}
#endif /* defined(OMR_OS_WINDOWS) && !defined(OMR_ENV_DATA64) */
};

#endif /*BITS_HPP_*/
