/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#if !defined(SEGREGATEDMARKINGSCHEME_HPP_)
#define SEGREGATEDMARKINGSCHEME_HPP_

#include "omrcomp.h"
#include "objectdescription.h"

#include "GCExtensionsBase.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "MarkingScheme.hpp"

#include "BaseVirtual.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_EnvironmentBase;

class MM_SegregatedMarkingScheme : public MM_MarkingScheme
{
	/*
	 * Data members
	 */
public:
protected:
private:
	/*
	 * Function members
	 */
public:
	static MM_SegregatedMarkingScheme *newInstance(MM_EnvironmentBase *env);
	void kill(MM_EnvironmentBase *env);
	
	MMINLINE void
	preMarkSmallCells(MM_EnvironmentBase* env, MM_HeapRegionDescriptorSegregated *containingRegion, uintptr_t *cellList, uintptr_t preAllocatedBytes)
	{
		if (NULL != cellList) {
			uintptr_t cellSize = containingRegion->getCellSize();

			uint8_t *objPtrLow = (uint8_t *)cellList;
			/* objPtrHigh is the last object (cell) to be premarked.
			 * => if there is only one object to premark than low will be equal to high
			 */
			uint8_t *objPtrHigh = (uint8_t *)cellList + preAllocatedBytes - cellSize;
			uintptr_t slotIndexLow, slotIndexHigh;
			uintptr_t bitMaskLow, bitMaskHigh;

			_markMap->getSlotIndexAndBlockMask((omrobjectptr_t)objPtrLow, &slotIndexLow, &bitMaskLow, false /* high bit block mask for low slot word */);
			_markMap->getSlotIndexAndBlockMask((omrobjectptr_t)objPtrHigh, &slotIndexHigh, &bitMaskHigh, true /* low bit block mask for high slot word */);

			if (slotIndexLow == slotIndexHigh) {
				_markMap->markBlockAtomic(slotIndexLow, bitMaskLow & bitMaskHigh);
			} else {
				_markMap->markBlockAtomic(slotIndexLow, bitMaskLow);
				_markMap->setMarkBlock(slotIndexLow + 1, slotIndexHigh - 1, (uintptr_t)-1);
				_markMap->markBlockAtomic(slotIndexHigh, bitMaskHigh);
			}
		}
	}
protected:
	/**
	 * Create a MM_RealtimeMarkingScheme object
	 */
	MM_SegregatedMarkingScheme(MM_EnvironmentBase *env)
		: MM_MarkingScheme(env)
	{
		_typeId = __FUNCTION__;
	}
private:
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* SEGREGATEDMARKINGSCHEME_HPP_ */
