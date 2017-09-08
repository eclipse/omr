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
