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


#if !defined(OBJECTALLOCATIONMODEL_HPP_)
#define OBJECTALLOCATIONMODEL_HPP_

#include "omrcfg.h"
#include "omr.h"
#include "ModronAssertions.h"
#include <string.h>

#include "AllocateDescription.hpp"
#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "MemorySpace.hpp"
#include "ObjectModel.hpp"

/**
 * Abstract class definition for the object allocation model.
 */
class MM_ObjectAllocationModel : public MM_BaseVirtual
{
public:
protected:
	MM_AllocateDescription *_allocDescription; /**< The allocation description associated with the current object allocation */
	MM_GCExtensionsBase* _extensions; /**< The GC extensions associated with the JVM */

private:
	
protected:
	MM_ObjectAllocationModel(MM_EnvironmentBase *env) :
		MM_BaseVirtual(),
		_allocDescription(NULL),
		_extensions(env->getExtensions())
	{
		_typeId = __FUNCTION__;
	};

	virtual bool initialize(MM_EnvironmentBase *env) { return true; }
	virtual void tearDown(MM_EnvironmentBase *env) { return; }
	
	MMINLINE uintptr_t adjustSizeInBytes(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired)
	{
		return _extensions->objectModel.adjustSizeInBytes(sizeInBytesRequired);
	}
	
	MMINLINE MM_MemorySpace *getMemorySpace(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
	{
		MM_MemorySpace *memorySpace = NULL;
		
		if (allocDescription->getTenuredFlag()) {
			/* For resman, string constants must be allocated out of the default memory space */
			memorySpace = env->getExtensions()->heap->getDefaultMemorySpace();
		} else {
			memorySpace = env->getMemorySpace();
		}
		
		allocDescription->setMemorySpace(memorySpace);
		
		return memorySpace;
	}
	
	
	/**
	 * Only called internally from the initialize*Object methods to memset the non-header components of the object.  Note that this function
	 * assumes that object is contiguous so it cannot be called to clear fields in arraylets or objlets.
	 */
	MMINLINE void
	internalClearFields(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, omrobjectptr_t objectPtr)
	{
		uintptr_t sizeInBytesRequired = allocDescription->getBytesRequested();
		/* Clear the object fields if necessary */
		if(shouldFieldsBeCleared(env, allocDescription)) {
			memset((void *)objectPtr, 0, sizeInBytesRequired);
		}
	}

	/**
	 * Check is clearing of fields is required
	 * It is not required if:
	 *  - an allocation is resolved from pre-zeroed TLH
	 */
	MMINLINE bool
	shouldFieldsBeCleared(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
	{
		/* NON_ZERO_TLH flag set is a request to not clear (not necesarily to allocate form non zero TLH) */
		bool shouldClear = !allocDescription->getNonZeroTLHFlag();
#if defined(OMR_GC_BATCH_CLEAR_TLH)
		/* Even if skipping to clear is not requested, we may decide to so, if the allocation comes from a batch cleared TLH */
		/* If allocation was not satisfied from TLH we ignore batchClearTLH, and simple obey the original condition */
		shouldClear = shouldClear && !(_extensions->batchClearTLH && allocDescription->isCompletedFromTlh());
#endif /* OMR_GC_BATCH_CLEAR_TLH */
		return shouldClear;
	}
	
public:
	
	void kill(MM_EnvironmentBase *env)
	{
		tearDown(env);
		env->getForge()->free(this);
	}
	
	/**
	 * Set the allocation description object for the current allocation.
	 */
	MMINLINE void setCurrentAllocDescription(MM_AllocateDescription *allocDescription) { _allocDescription = allocDescription; }
};
#endif /* OBJECTALLOCATIONMODEL_HPP_ */
