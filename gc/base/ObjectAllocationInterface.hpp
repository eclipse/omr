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


/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(OBJECTALLOCATIONINTERFACE_HPP_)
#define OBJECTALLOCATIONINTERFACE_HPP_

#include "omrcfg.h"
#include "modronbase.h"
#include "ModronAssertions.h"

#include "BaseVirtual.hpp"
#include "AllocationStats.hpp"

class MM_AllocateDescription;
class MM_EnvironmentBase;
class MM_FrequentObjectsStats;
class MM_MemoryPool;
class MM_MemorySpace;
class MM_MemorySubSpace;

/**
 * Abstract class definition for the object allocation interface.
 * This class represents the per-thread type associated to each MM_Environment that controls how a
 * thread allocates objects.  This control includes any caching measures (e.g., TLH).
 */
class MM_ObjectAllocationInterface : public MM_BaseVirtual
{
/*
 * Data members
 */
private:

protected:
	MM_EnvironmentBase *_owningEnv;  /**< The environment with which the receiver is associated */
	MM_AllocationStats _stats; /**< Allocation statistics for this allocation interface. */
	MM_FrequentObjectsStats* _frequentObjectsStats;

public:

/*
 * Function members
 */
private:

protected:
	/**
	 * Provide an initialization for the class
	 *
	 * @param env environment
	 * @return true if an initialization is successful
	 */
	virtual bool initialize(MM_EnvironmentBase *env) = 0;

	/**
	 * Tear Down for the class
	 *
	 * @param env environment
	 */
	virtual void tearDown(MM_EnvironmentBase *env) = 0;

	MM_ObjectAllocationInterface(MM_EnvironmentBase *env) :
		MM_BaseVirtual(),
		_owningEnv(env)
		,_stats()
		,_frequentObjectsStats(NULL)
	{
		_typeId = __FUNCTION__;
	};

public:
	MM_AllocationStats* getAllocationStats() { return &_stats; }
	MM_FrequentObjectsStats* getFrequentObjectsStats() { return _frequentObjectsStats; }
	MM_EnvironmentBase *getOwningEnv() { return _owningEnv; }

	virtual void kill(MM_EnvironmentBase *env) = 0;

	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure) = 0;
	virtual void *allocateArray(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure) = 0;
#if defined(OMR_GC_ARRAYLETS)
	/**
	 * Allocate the arraylet spine.
	 */
	virtual void *
	allocateArrayletSpine(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
	{
		Assert_MM_unreachable();
		return NULL;
	}

	/**
	 * Allocate an arraylet leaf.
	 */
	virtual void *
	allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
	{
		Assert_MM_unreachable();
		return NULL;
	}
#endif /* OMR_GC_ARRAYLETS */

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	virtual void *allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *memorySubSpace, MM_MemoryPool *memoryPool);
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	virtual void flushCache(MM_EnvironmentBase *env);
	virtual void reconnectCache(MM_EnvironmentBase *env);
	virtual void restartCache(MM_EnvironmentBase *env);
	
	virtual void enableCachedAllocations(MM_EnvironmentBase* env) {};
	virtual void disableCachedAllocations(MM_EnvironmentBase* env) {};
	virtual bool cachedAllocationsEnabled(MM_EnvironmentBase* env) { return true; }
	
};
#endif /* OBJECTALLOCATIONINTERFACE_HPP_ */
