/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Modron_Metronome
 */

#if !defined(CONFIGURATIONSEGREGATED_HPP_)
#define CONFIGURATIONSEGREGATED_HPP_

#include "omrcfg.h"

#include "Configuration.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_EnvironmentBase;
class MM_GlobalAllocationManagerSegregated;
class MM_GlobalCollector;
class MM_Heap;
class MM_SegregatedAllocationTracker;

class MM_ConfigurationSegregated : public MM_Configuration
{
	/*
	 * Data members
	 */
public:
protected:
private:
	static const uintptr_t SEGREGATED_REGION_SIZE_BYTES = (64 * 1024);
	static const uintptr_t SEGREGATED_ARRAYLET_LEAF_SIZE_BYTES = OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES;

	/*
	 * Function members
	 */
public:
	static MM_Configuration *newInstance(MM_EnvironmentBase *env);

	virtual MM_GlobalCollector *createGlobalCollector(MM_EnvironmentBase *env);
	virtual MM_Heap *createHeapWithManager(MM_EnvironmentBase *env, uintptr_t heapBytesRequested, MM_HeapRegionManager *regionManager);
	virtual MM_HeapRegionManager *createHeapRegionManager(MM_EnvironmentBase *env);
	virtual MM_MemorySpace *createDefaultMemorySpace(MM_EnvironmentBase *env, MM_Heap *heap, MM_InitializationParameters *parameters);
	virtual J9Pool *createEnvironmentPool(MM_EnvironmentBase *env);
	virtual MM_Dispatcher *createDispatcher(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize);
	
	virtual void defaultMemorySpaceAllocated(MM_GCExtensionsBase *extensions, void* defaultMemorySpace);
	
	MM_ConfigurationSegregated(MM_EnvironmentBase *env)
		: MM_Configuration(env, gc_policy_metronome, mm_regionAlignment, SEGREGATED_REGION_SIZE_BYTES, SEGREGATED_ARRAYLET_LEAF_SIZE_BYTES, gc_modron_wrtbar_none, gc_modron_allocation_type_segregated)
	{
		_typeId = __FUNCTION__;
	};

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	virtual MM_EnvironmentBase *allocateNewEnvironment(MM_GCExtensionsBase *extensions, OMR_VMThread *omrVMThread);
	virtual bool initializeEnvironment(MM_EnvironmentBase *env);

private:
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* CONFIGURATIONSEGREGATED_HPP_ */
