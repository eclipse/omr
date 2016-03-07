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

#include "Math.hpp"

/**
 * Subtract subtrahend from minuend if the result will be positive. Zero otherwise.
 * @param minuend - the larger value
 * @param subtrahend - the smaller value
 * @return the difference between minuend and subtrahend, or zero if minuend is not larger than subtrahend
 */
uintptr_t
MM_Math::saturatingSubtract(uintptr_t minuend, uintptr_t subtrahend) 
{
	uintptr_t result = 0;
	if (minuend > subtrahend) {
		result = minuend - subtrahend;
	}
	return result;
}

/**
 * Return the weighted average through combining the new value to the current value.
 * @return the weighted average of the combined parameters.
 */
float
MM_Math::weightedAverage(float currentAverage, float newValue, float weight) 
{
	return ((currentAverage) * weight) + ((newValue) * ((float)1.0 - weight));
}
