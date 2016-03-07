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

#include "Base.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "ModronAssertions.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

class MM_ContractSlotScanner : public MM_Base
{
private:
	void *_srcBase;
	void *_srcTop;
	void *_dstBase;
protected:
public:

private:
protected:
public:
	MM_ContractSlotScanner(MM_EnvironmentBase *env, void *srcBase, void *srcTop, void *dstBase) :
		MM_Base()
		,_srcBase(srcBase)
		,_srcTop(srcTop)
		,_dstBase(dstBase)
	{}

	virtual void
	doSlot(omrobjectptr_t *slotPtr)
	{
		omrobjectptr_t objectPtr = *slotPtr;
		if(NULL != objectPtr) {
			if((objectPtr >= (omrobjectptr_t)_srcBase) && (objectPtr < (omrobjectptr_t)_srcTop)) {
				objectPtr = (omrobjectptr_t)((((uintptr_t)objectPtr) - ((uintptr_t)_srcBase)) + ((uintptr_t)_dstBase));
				*slotPtr = objectPtr;
			}
		}
	}

	void
	scanAllSlots(MM_EnvironmentBase *env)
	{
#error provide implementation
	}

	/* TODO remove this function as it is Java specific */
	void
	setIncludeStackFrameClassReferences(bool includeStackFrameClassReferences)
	{
		/* do nothing */
	}
};

#endif /* OMR_GC_MODRON_SCAVENGER */
