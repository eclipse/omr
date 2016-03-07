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


#if !defined(GLOBALALLOCATIONMANAGER_HPP_)
#define GLOBALALLOCATIONMANAGER_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "ModronAssertions.h"

class MM_AllocationContext;

class MM_GlobalAllocationManager : public MM_BaseVirtual
{
/* Data members & types */
public:
protected:
	MM_GCExtensionsBase * const _extensions;	/**< A cache of the global extensions */
	uintptr_t _managedAllocationContextCount; /**< The number of allocation contexts which will be instantiated and managed by the GlobalAllocationManager that are part of the _managedAllocationContexts array, other allocation contexts may exist in certain specs */
	uintptr_t _nextAllocationContext;
	MM_AllocationContext **_managedAllocationContexts;
private:
	
/* Methods */
public:
	virtual void kill(MM_EnvironmentBase *env) = 0;
	
	/**
	 * Acquires an allocation context for the thread represented by the given env (modifies the env)
	 * @return true on successful acquisition of allocation context
	 */
	virtual bool acquireAllocationContext(MM_EnvironmentBase *env) = 0;
	/**
	 * Returns the env's allocation context to the receiver (modifies the env)
	 */
	virtual void releaseAllocationContext(MM_EnvironmentBase *env) = 0;
	/**
	 * Print current counters for AC stats and resets the counters afterwards
	 */	
	virtual void printAllocationContextStats(MM_EnvironmentBase *env, uintptr_t eventNum, J9HookInterface** hookInterface) = 0;

	/**
	 * Instructs the receiver to call flush on all the contexts it manages (meant to be called prior to collect).
	 */
	virtual void flushAllocationContexts(MM_EnvironmentBase *env);
	
	/**
	 * Instructs the receiver to call flushForShutdown on all the contexts it manages (meant to be called prior to shut down).
	 */
	virtual void flushAllocationContextsForShutdown(MM_EnvironmentBase *env);


	/**
	 * Return the allocation context as specified by the caller.
	 * @param index Allocation context index to return.
	 * @return An allocation context.
	 */
	virtual MM_AllocationContext *getAllocationContextByIndex(uintptr_t index) {
		Assert_MM_true(index < _managedAllocationContextCount);
		return _managedAllocationContexts[index];
	}

protected:
	bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	MM_GlobalAllocationManager(MM_EnvironmentBase *env)
		: MM_BaseVirtual()
		, _extensions(env->getExtensions())
		, _managedAllocationContextCount(0)
		, _nextAllocationContext(0)
		, _managedAllocationContexts(NULL)
	{
		_typeId = __FUNCTION__;
	};
private:
};

#endif /* GLOBALALLOCATIONMANAGER_HPP_ */
