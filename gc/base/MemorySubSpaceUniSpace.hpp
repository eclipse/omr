/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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


/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(MEMORYSUBSPACEUNISPACE_HPP_)
#define MEMORYSUBSPACEUNISPACE_HPP_

#include "MemorySubSpace.hpp"

#define HEAP_FREE_RATIO_EXPAND_DIVISOR		100
#define HEAP_FREE_RATIO_EXPAND_MULTIPLIER	17

/**
 * Functionality that is common for Flat/Concurrent but not applicable for SemiSpace.
 * An abstract class that should not be instantiated.
 * @ingroup GC_Base_Core
 */
class MM_MemorySubSpaceUniSpace : public MM_MemorySubSpace
{
protected:
	uintptr_t adjustExpansionWithinFreeLimits(MM_EnvironmentBase *env, uintptr_t expandSize);
	uintptr_t checkForRatioExpand(MM_EnvironmentBase *env, uintptr_t bytesRequired);	
	bool checkForRatioContract(MM_EnvironmentBase *env);
	uintptr_t calculateExpandSize(MM_EnvironmentBase *env, uintptr_t bytesRequired, bool expandToSatisfy);
	uintptr_t calculateCollectorExpandSize(MM_EnvironmentBase *env, MM_Collector *requestCollector, MM_AllocateDescription *allocDescription);
	uintptr_t calculateTargetContractSize(MM_EnvironmentBase *env, uintptr_t allocSize, bool ratioContract);
	bool timeForHeapContract(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool systemGC);
	bool timeForHeapExpand(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);	
	uintptr_t performExpand(MM_EnvironmentBase *env);
	uintptr_t performContract(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);
	uintptr_t getHeapFreeMaximumHeuristicMultiplier(MM_EnvironmentBase *env);
	uintptr_t getHeapFreeMinimumHeuristicMultiplier(MM_EnvironmentBase *env);

public:
	virtual void checkResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription = NULL, bool _systemGC = false);
	virtual intptr_t performResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription = NULL);

	/**
	 * Create a MemorySubSpaceUniSpace object.
	 */
	MM_MemorySubSpaceUniSpace(MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena,
		bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize, uintptr_t memoryFlags, uint32_t objectFlags)
	:
		MM_MemorySubSpace(env, NULL, physicalSubArena, usesGlobalCollector, minimumSize, initialSize, maximumSize, memoryFlags, objectFlags)
	{
		_typeId = __FUNCTION__;
	};
};
 
#endif /* MEMORYSUBSPACEUNISPACE_HPP_ */
