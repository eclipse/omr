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

#if !defined(ALLOCATIONINTERFACEGENERiC_HPP_)
#define ALLOCATIONINTERFACEGENERiC_HPP_

#include "omr.h"

#include "ObjectAllocationInterface.hpp"

class MM_AllocationInterfaceGeneric : public MM_ObjectAllocationInterface
{
public:
protected:
private:
	bool _cachedAllocationsEnabled; /**< Are cached allocations enabled? */

public:
	static MM_AllocationInterfaceGeneric* newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
	virtual void *allocateArray(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
#if defined(OMR_GC_ARRAYLETS)
	virtual void *allocateArrayletSpine(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
#endif /* OMR_GC_ARRAYLETS */

	virtual void flushCache(MM_EnvironmentBase *env);
	virtual void reconnectCache(MM_EnvironmentBase *env);

	virtual void enableCachedAllocations(MM_EnvironmentBase* env);
	virtual void disableCachedAllocations(MM_EnvironmentBase* env);
	virtual bool cachedAllocationsEnabled(MM_EnvironmentBase* env) { return _cachedAllocationsEnabled; }

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	MM_AllocationInterfaceGeneric(MM_EnvironmentBase *env) :
		MM_ObjectAllocationInterface(env),
		_cachedAllocationsEnabled(false)
	{
		_typeId = __FUNCTION__;
	};

private:

};

#endif /*ALLOCATIONINTERFACEGENERiC_HPP_*/
