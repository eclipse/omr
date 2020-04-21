/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#if !defined(TLHALLOCATIONINTERFACE_HPP_)
#define TLHALLOCATIONINTERFACE_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrmodroncore.h"

#include "ObjectAllocationInterface.hpp"
#include "TLHAllocationSupport.hpp"

class MM_AllocateDescription;
class MM_EnvironmentBase;
class MM_MemoryPool;
class MM_MemorySpace;
class MM_MemorySubSpace;

#if defined(OMR_GC_THREAD_LOCAL_HEAP)

/**
 * Object allocation interface definition for TLH style allocators.
 * Implementation of the Thread Local Heap allocation style, each thread receiving a per-instance
 * version of the type to manage its caches.
 * @seealso MM_ObjectAllocationInterface
 */
class MM_TLHAllocationInterface : public MM_ObjectAllocationInterface
{
public:
protected:
private:
	MM_EnvironmentBase *_owningEnv;  /**< The environment with which the receiver is associated */
	MM_TLHAllocationSupport _tlhAllocationSupport; /**< TLH Allocation sub interface class */

#if defined(OMR_GC_NON_ZERO_TLH)
	MM_TLHAllocationSupport _tlhAllocationSupportNonZero; /**< TLH Allocation sub interface class */
#endif /* defined(OMR_GC_NON_ZERO_TLH) */

	bool _cachedAllocationsEnabled; /**< Are cached allocations enabled? */
	uintptr_t _bytesAllocatedBase; /**< Bytes allocated at the start of an allocation request.  Relative to _stats.bytesAllocated(). */

public:
	static MM_TLHAllocationInterface *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
	virtual void *allocateArray(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
	virtual void *allocateArrayletSpine(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);

	virtual void *allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *memorySubSpace, MM_MemoryPool *memoryPool);

	virtual void flushCache(MM_EnvironmentBase *env);
	virtual void restartCache(MM_EnvironmentBase *env);
	
	/* BEN TODO: Collapse the env->enable/disableInlineTLHAllocate with these enable/disableCachedAllocations */
	virtual void enableCachedAllocations(MM_EnvironmentBase* env) { _cachedAllocationsEnabled = true; }
	virtual void disableCachedAllocations(MM_EnvironmentBase* env) { _cachedAllocationsEnabled = false; };
	virtual bool cachedAllocationsEnabled(MM_EnvironmentBase* env) { return _cachedAllocationsEnabled; }

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

private:
	void reconnect(MM_EnvironmentBase *env, bool shouldFlush);
	void *allocateFromTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool shouldCollectOnFailure);

	/**
	 * Create a ThreadLocalHeap object.
	 */
	MM_TLHAllocationInterface(MM_EnvironmentBase *env) :
		MM_ObjectAllocationInterface(env),
		_owningEnv(env),
		_tlhAllocationSupport(env, true),
#if defined(OMR_GC_NON_ZERO_TLH)
		_tlhAllocationSupportNonZero(env, false),
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
		_cachedAllocationsEnabled(true),
		_bytesAllocatedBase(0)
	{
		_typeId = __FUNCTION__;
		_tlhAllocationSupport._objectAllocationInterface = this;

#if defined(OMR_GC_NON_ZERO_TLH)
		_tlhAllocationSupportNonZero._objectAllocationInterface = this;
#endif /* defined(OMR_GC_NON_ZERO_TLH) */
	};
};

#endif /* OMR_GC_THREAD_LOCAL_HEAP */
#endif /* TLHALLOCATIONINTERFACE_HPP_ */
