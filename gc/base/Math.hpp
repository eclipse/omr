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
 ******************************************************************************/

#if !defined(MATH_HPP_)
#define MATH_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"

class MM_Math
{
public:
	static uintptr_t saturatingSubtract(uintptr_t minuend, uintptr_t subtrahend);
	
	static float weightedAverage(float currentAverage, float newValue, float weight);

	/* Round value up */
	static MMINLINE uintptr_t roundToCeiling(uintptr_t granularity, uintptr_t number) {
		return number + ((number % granularity) ? (granularity - (number % granularity)) : 0);
	}
	
	/* Round value up */
	static MMINLINE uint64_t roundToCeilingU64(uint64_t granularity, uint64_t number) {
		return number + ((number % granularity) ? (granularity - (number % granularity)) : 0);
	}

	/* Round value down */
	static MMINLINE uintptr_t roundToFloor(uintptr_t granularity, uintptr_t number) {
		return number - (number % granularity);
	}
	
	/* Round value down */
	static MMINLINE uint64_t roundToFloorU64(uint64_t granularity, uint64_t number) {
		return number - (number % granularity);
	}

	/* Round value to nearlest uintptr_t aligned */
	static MMINLINE uintptr_t roundToSizeofUDATA(uintptr_t number) {
		return (number + (sizeof(uintptr_t) - 1)) & (~(sizeof(uintptr_t) - 1));
	}
	
	/* Round value to nearlest uint32_t aligned */
	static MMINLINE uintptr_t roundToSizeofU32(uintptr_t number) {
		return (number + (sizeof(uint32_t) - 1)) & (~(sizeof(uint32_t) - 1));
	}

	/* returns:	 0 for 0
	 * 			 0 for 1
	 * 			 1 for [2,4)
	 * 			 2 for [4, 8)
	 *			...
	 * 			30 for [U32_MAX/4, U32_MAX/2)
	 * 			31 for [U32_MAX/2, U32_MAX] 
	 * 			...
	 * 			62 for [U64_MAX/4, U64_MAX/2)
	 * 			63 for [U64_MAX/2, U64_MAX] 
	 */
	static uintptr_t floorLog2(uintptr_t n) {
		uintptr_t pos = 0;
#ifdef OMR_ENV_DATA64
		if (n >= ((uintptr_t)1 << 32)) {
			n >>=  32;
			pos +=  32;
		}
#endif
		if (n >= (1<< 16)) {
			n >>=  16;
			pos +=  16;
		}		
		if (n >= (1<< 8)) {
			n >>=  8;
			pos +=  8;
		}
		if (n >= (1<< 4)) {
			n >>=  4;
			pos +=  4;
		}
		if (n >= (1<< 2)) {
			n >>=  2;
			pos +=  2;
		}
		if (n >= (1<< 1)) {
			pos +=  1;
		}
		return pos;
	}

	/**< return absolute value of a signed integer */
	static MMINLINE intptr_t abs(intptr_t number) {
		if (number < 0) {
			number = -number;
		}

		return number;
	}
};

#endif /*MATH_HPP_*/
