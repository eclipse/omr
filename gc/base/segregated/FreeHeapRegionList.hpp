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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(FREEHEAPREGIONLIST_HPP_)
#define FREEHEAPREGIONLIST_HPP_

#if defined(OMR_GC_SEGREGATED_HEAP)

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronopt.h"
#include "sizeclasses.h"

#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "HeapRegionList.hpp"
#include "HeapRegionQueue.hpp"
#include "SizeClasses.hpp"

/**
 * A HeapRegionList on which every Region stands for a contiguous range of regions (possibly of length one).
 */
class MM_FreeHeapRegionList : public MM_HeapRegionList
{
/* Data members & types */		
public:	
protected:
private:
	
/* Methods */	
public:

	virtual void kill(MM_EnvironmentBase *env) = 0;
	
	virtual bool initialize(MM_EnvironmentBase *env) = 0;
	virtual void tearDown(MM_EnvironmentBase *env) = 0;

	MM_FreeHeapRegionList(MM_HeapRegionList::RegionListKind regionListKind, bool singleRegionsOnly) :
		MM_HeapRegionList(regionListKind, singleRegionsOnly)
	{
		_typeId = __FUNCTION__;
	}

	virtual void push(MM_HeapRegionDescriptorSegregated *region) = 0;
	virtual void push(MM_HeapRegionQueue *src) = 0;
	virtual void push(MM_FreeHeapRegionList *src) = 0;
	
	virtual MM_HeapRegionDescriptorSegregated* pop() = 0;

	/*
	 * This method must be used with care.  
	 * In particular, it is wrong to detach from a list
	 * while iterating over it unless the detach stops further iteration.
	 */
	virtual void detach(MM_HeapRegionDescriptorSegregated *cur) = 0;

	virtual MM_HeapRegionDescriptorSegregated *allocate(MM_EnvironmentBase *env, uintptr_t szClass, uintptr_t numRegions, uintptr_t maxExcess) = 0;

	MM_HeapRegionDescriptorSegregated *allocate(MM_EnvironmentBase *env, uintptr_t szClass)
	{
		assert(_singleRegionsOnly);
		MM_HeapRegionDescriptorSegregated *region = pop();
		if (NULL != region) {
			region->setRangeHead(region);
			if (szClass == OMR_SIZECLASSES_LARGE) {
				region->setLarge(1);
			} else if (szClass == OMR_SIZECLASSES_ARRAYLET) {
				region->setArraylet();
			} else {
				region->setSmall(szClass);
			}
		}
		return region;
	}
		
	/* Methods inherited from HeapRegionList */
	virtual bool isEmpty() { return 0 == _length; }
	virtual uintptr_t getTotalRegions() = 0;
	virtual void showList(MM_EnvironmentBase *env) = 0;
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* FREEHEAPREGIONLIST_HPP_ */
