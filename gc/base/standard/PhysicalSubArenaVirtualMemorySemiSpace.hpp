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

#if !defined(PHYSICALSUBARENAVIRTUALMEMORYSEMISPACE_HPP_)
#define PHYSICALSUBARENAVIRTUALMEMORYSEMISPACE_HPP_

#include "omrcfg.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "PhysicalSubArenaVirtualMemory.hpp"

class MM_EnvironmentBase;
class MM_HeapRegionDescriptor;
class MM_MemorySubSpace;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_PhysicalSubArenaVirtualMemorySemiSpace : public MM_PhysicalSubArenaVirtualMemory
{
private:
	bool _resizable; /**< determines if the semi space is resizable */
	bool _avoidMovingObjects; /**< VMDESIGN 1690: avoid moving objects during contract where possible */

	uintptr_t calculateExpansionSplit(MM_EnvironmentBase *env, uintptr_t requestExpandSize, uintptr_t *allocateSpaceSize, uintptr_t *survivorSpaceSize);

protected:
	MM_HeapRegionDescriptor *_lowSemiSpaceRegion;
	MM_HeapRegionDescriptor *_highSemiSpaceRegion;

	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

public:
	static MM_PhysicalSubArenaVirtualMemorySemiSpace *newInstance(MM_EnvironmentBase *env, MM_Heap *heap);
	virtual void kill(MM_EnvironmentBase *env);

	virtual bool inflate(MM_EnvironmentBase *env);

	virtual uintptr_t expand(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual bool canExpand(MM_EnvironmentBase *env);
	virtual uintptr_t expandNoCheck(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual uintptr_t checkCounterBalanceExpand(MM_EnvironmentBase *env, uintptr_t expandSizeDeltaAlignment, uintptr_t expandSize);

	virtual uintptr_t contract(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual bool canContract(MM_EnvironmentBase *env);

	virtual void tilt(MM_EnvironmentBase *env, uintptr_t allocateSpaceSize, uintptr_t survivorSpaceSize);
	virtual void tilt(MM_EnvironmentBase *env, uintptr_t survivorSpaceSizeRequest);

	MM_PhysicalSubArenaVirtualMemorySemiSpace(MM_Heap *heap) :
		MM_PhysicalSubArenaVirtualMemory(heap),
		_lowSemiSpaceRegion(NULL),
		_highSemiSpaceRegion(NULL)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

#endif /* PHYSICALSUBARENAVIRTUALMEMORYSEMISPACE_HPP_ */
