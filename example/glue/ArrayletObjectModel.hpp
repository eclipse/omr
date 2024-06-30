/*******************************************************************************
 * Copyright IBM Corp. and others 1991
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(ARRAYLETOBJECTMODEL_)
#define ARRAYLETOBJECTMODEL_

#include "omrcfg.h"
#include "ModronAssertions.h"


class MM_GCExtensionsBase;
class MM_MemorySubSpace;
class MM_ForwardedHeader;

class GC_ArrayletObjectModel
{
	/*
	 * Function members
	 */
private:
protected:
public:
	bool
	initialize(MM_GCExtensionsBase *extensions)
	{
		return true;
	}

	void tearDown(MM_GCExtensionsBase *extensions) {}

	MMINLINE fomrobject_t *
	getArrayoidPointer(omrarrayptr_t arrayPtr)
	{
		return (fomrobject_t *) NULL;
	}

	MMINLINE void
	expandArrayletSubSpaceRange(MM_MemorySubSpace * subSpace, void * rangeBase, void * rangeTop, uintptr_t largestDesirableArraySpineSize)
	{
		/* No-op */
	}

	MMINLINE void
	fixupDataAddr(MM_ForwardedHeader *forwardedHeader, omrobjectptr_t arrayPtr)
	{
#if defined(OMR_ENV_DATA64)
		Assert_MM_unreachable();
#endif /* defined(OMR_ENV_DATA64) */
	}
	
	/**
	 * Returns the size of an indexable object, in bytes, including the header.
	 * @param arrayPtr Pointer to the indexable object whose size is required
	 * @return Size of object in bytes including the header
	 */
	MMINLINE uintptr_t
	getSizeInBytesWithHeader(omrarrayptr_t arrayPtr)
	{
		Assert_MM_unimplemented();
		return 0;
	}
#if defined(OMR_GC_DOUBLE_MAP_ARRAYLETS)
	MMINLINE bool
	isDoubleMappingEnabled()
	{
		return false;
	}
#endif /* defined(OMR_GC_DOUBLE_MAP_ARRAYLETS) */

	MMINLINE bool
	isVirtualLargeObjectHeapEnabled()
	{
		return false;
	}

};

#endif /* ARRAYLETOBJECTMODEL_ */
