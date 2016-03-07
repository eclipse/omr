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
 ******************************************************************************/

/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(ALLOCATIONCONTEXT_HPP_)
#define ALLOCATIONCONTEXT_HPP_

#include "omrcomp.h"
#include "modronopt.h"

#include "BaseVirtual.hpp"
#include "MemorySubSpace.hpp"

#include "ModronAssertions.h"

class MM_EnvironmentBase;
class MM_AllocateDescription;
class MM_ObjectAllocationInterface;


class MM_AllocationContext : public MM_BaseVirtual
{
/* Data members / Types */
public:
protected:
private:
	
/* Methods */
public:
	void kill(MM_EnvironmentBase *env);
	/**
	 * Instructs the receiver to invalidate its cache.
	 */
	virtual void flush(MM_EnvironmentBase *env) = 0;
	
	/**
	 * Instructs the receiver to invalidate its cache in preparation for shutting down.
	 */
	virtual void flushForShutdown(MM_EnvironmentBase *env) = 0;

	/**
	 * Perform an allocate of the given allocationType and return it.
	 * @note No collect and climb will occur - the allocation must be satisfied within the specified location and will report a failure otherwise (implementation decision).
	 *
	 * @param[in] env The calling thread
	 * @param[in] objectAllocationInterface The allocation interface through which the original allocation call was initiated (only used by TLH allocations, can be NULL in other cases)
	 * @param[in] allocateDescription The description of the requested allocation
	 * @param[in] allocationType The type of allocation to perform
	 *
	 * @return The result of the allocation (NULL on failure)
	 */
	virtual void *allocate(MM_EnvironmentBase *env, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, MM_MemorySubSpace::AllocationType allocationType)
	{
		Assert_MM_unreachable();
		return NULL;
	}

	/**
	 * Ideally, this would only be understood by sub-classes which know about TLH allocation but we will use runtime assertions to ensure this is safe, for now
	 */
	virtual void *allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_ObjectAllocationInterface *objectAllocationInterface, bool shouldCollectOnFailure)
	{
		Assert_MM_unreachable();
		return NULL;
	}
	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, bool shouldCollectOnFailure)
	{
		Assert_MM_unreachable();
		return NULL;
	}
#if defined(OMR_GC_ARRAYLETS)
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, bool shouldCollectOnFailure)
	{
		Assert_MM_unreachable();
		return NULL;
	}
#endif /* defined(OMR_GC_ARRAYLETS) */		

protected:
	virtual void tearDown(MM_EnvironmentBase *env);
	bool initialize(MM_EnvironmentBase *env);
	MM_AllocationContext()
		: MM_BaseVirtual()
	{
		_typeId = __FUNCTION__;
	}
private:
};


#endif /* ALLOCATIONCONTEXT_HPP */


