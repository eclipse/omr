/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#if !defined(ALLOCATIONFAILURESTATS_HPP_)
#define ALLOCATIONFAILURESTATS_HPP_

#include "omrcomp.h"

#include "Base.hpp"

/**
 * @todo Provide class documentation
 * @ingroup GC_Stats
 */
class MM_AllocationFailureStats : public MM_Base 
{
public:
	uintptr_t subSpaceType;
	uintptr_t allocationFailureSize;
	uintptr_t allocationFailureCount;

	/**
	 * Create a AllocationFailureStats object.
	 */   
	MM_AllocationFailureStats() :
		MM_Base(),
		subSpaceType(0),
		allocationFailureSize(0),
		allocationFailureCount(0)
	{};
};

#endif /* ALLOCATIONFAILURESTATS_HPP_ */
