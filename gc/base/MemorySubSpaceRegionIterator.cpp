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

#include "omrcfg.h"

#include "MemorySubSpaceRegionIterator.hpp"

#include "HeapRegionDescriptor.hpp"
#include "MemorySubSpace.hpp"

GC_MemorySubSpaceRegionIterator::GC_MemorySubSpaceRegionIterator(MM_MemorySubSpace* subspace) :
	_leafStackSlot(0)
	,_region(NULL)
{
	_subSpaceStack[0] = subspace;
	initializeStack(0);
}

void
GC_MemorySubSpaceRegionIterator::initializeStack(uintptr_t fromStackSlot)
{
	_leafStackSlot = fromStackSlot;
	while (_subSpaceStack[_leafStackSlot]->getChildren()) {
		_leafStackSlot += 1;
		Assert_MM_true(_leafStackSlot < MAX_STACK_SLOTS);
		_subSpaceStack[_leafStackSlot] = _subSpaceStack[_leafStackSlot - 1]->getChildren();
	}
	_region = _subSpaceStack[_leafStackSlot]->getFirstRegion();
}

MM_HeapRegionDescriptor *
GC_MemorySubSpaceRegionIterator::nextRegion()
{
	MM_HeapRegionDescriptor *currentRegion = NULL;
	if (NULL != _region) {
		currentRegion = _region;
		/* try first from most nested subspace on the stack */
		_region = _subSpaceStack[_leafStackSlot]->getNextRegion(currentRegion);
		if (NULL == _region) {
			/* find lowest (in hierarchy) subspace, excluding root, that has a sibling */
			uintptr_t stackSlot = _leafStackSlot;
			for (; stackSlot > 0; stackSlot--) {
				if (NULL != _subSpaceStack[stackSlot]->getNext()) {
					/* found sibling, move to it */
					_subSpaceStack[stackSlot] = _subSpaceStack[stackSlot]->getNext();
					break;
				}
			}
			if (stackSlot > 0) {
				/* anything lower (in hierarchy) should get reset (and than we get the next region from the leaf subspace) */
				initializeStack(stackSlot);
			}
		}
	}
	return currentRegion;
}
