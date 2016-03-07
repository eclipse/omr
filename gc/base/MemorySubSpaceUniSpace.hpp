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
	uintptr_t adjustExpansionWithinSoftMax(MM_EnvironmentBase *env, uintptr_t expandSize, uintptr_t minimumBytesRequired);
	uintptr_t checkForRatioExpand(MM_EnvironmentBase *env, uintptr_t bytesRequired);	
	bool checkForRatioContract(MM_EnvironmentBase *env);
	uintptr_t calculateExpandSize(MM_EnvironmentBase *env, uintptr_t bytesRequired, bool expandToSatisfy);
	uintptr_t calculateCollectorExpandSize(MM_EnvironmentBase *env, MM_Collector *requestCollector, MM_AllocateDescription *allocDescription);
	uintptr_t calculateTargetContractSize(MM_EnvironmentBase *env, uintptr_t allocSize, bool ratioContract);
	bool timeForHeapContract(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool systemGC);
	bool timeForHeapExpand(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);	
	uintptr_t performExpand(MM_EnvironmentBase *env);
	uintptr_t performContract(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);

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
