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

#if !defined(ARRAYLETOBJECTMODEL_)
#define ARRAYLETOBJECTMODEL_

#include "omrcfg.h"

#if defined(OMR_GC_ARRAYLETS)

class MM_GCExtensionsBase;
class MM_MemorySubSpace;

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
};

#endif /*OMR_GC_ARRAYLETS */
#endif /* ARRAYLETOBJECTMODEL_ */
