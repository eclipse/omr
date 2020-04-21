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
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, bool shouldCollectOnFailure)
	{
		Assert_MM_unreachable();
		return NULL;
	}

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


