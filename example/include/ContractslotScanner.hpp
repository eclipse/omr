/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
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
		Assert_MM_unimplemented();
	}

	/* TODO remove this function as it is Java specific */
	void
	setIncludeStackFrameClassReferences(bool includeStackFrameClassReferences)
	{
		/* do nothing */
	}
};

#endif /* OMR_GC_MODRON_SCAVENGER */
