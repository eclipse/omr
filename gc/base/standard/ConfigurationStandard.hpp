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
 * @ingroup GC_Modron_Standard
 */

#if !defined(CONFIGURATIONSTANDARD_HPP_)
#define CONFIGURATIONSTANDARD_HPP_

#include "omrcfg.h"

#include "Configuration.hpp"
#include "EnvironmentBase.hpp"

#if defined(OMR_GC_MODRON_STANDARD)

class MM_GlobalCollector;
class MM_Heap;
class MM_MemoryPool;

class MM_ConfigurationStandard : public MM_Configuration {
	/* Data members / Types */
public:
protected:
	/* Create Memory Pool(s)
	 * @param appendCollectorLargeAllocateStats - if true, configure the pool to append Collector allocates to Mutator allocates (default is to gather only Mutator)
	 */
	virtual MM_MemoryPool* createMemoryPool(MM_EnvironmentBase* env, bool appendCollectorLargeAllocateStats);
	virtual void tearDown(MM_EnvironmentBase* env);

	bool createSweepPoolManagerAddressOrderedList(MM_EnvironmentBase* env);
	bool createSweepPoolManagerSplitAddressOrderedList(MM_EnvironmentBase* env);
	bool createSweepPoolManagerHybrid(MM_EnvironmentBase* env);

	static const uintptr_t STANDARD_REGION_SIZE_BYTES = 64 * 1024;
	static const uintptr_t STANDARD_ARRAYLET_LEAF_SIZE_BYTES = UDATA_MAX;

private:

	/* Methods */
public:
	virtual MM_GlobalCollector* createGlobalCollector(MM_EnvironmentBase* env);
	virtual MM_Heap* createHeapWithManager(MM_EnvironmentBase* env, uintptr_t heapBytesRequested, MM_HeapRegionManager* regionManager);
	virtual MM_HeapRegionManager* createHeapRegionManager(MM_EnvironmentBase* env);
	virtual J9Pool* createEnvironmentPool(MM_EnvironmentBase* env);

	MM_ConfigurationStandard(MM_EnvironmentBase* env, MM_GCPolicy gcPolicy, uintptr_t regionSize)
		: MM_Configuration(env, gcPolicy, mm_regionAlignment, regionSize, STANDARD_ARRAYLET_LEAF_SIZE_BYTES, getWriteBarrierType(env), gc_modron_allocation_type_tlh)
	{
		_typeId = __FUNCTION__;
	};

protected:
	virtual bool initialize(MM_EnvironmentBase* env);
	virtual MM_EnvironmentBase* allocateNewEnvironment(MM_GCExtensionsBase* extensions, OMR_VMThread* omrVMThread);

private:
	static MM_GCWriteBarrierType getWriteBarrierType(MM_EnvironmentBase* env)
	{
		MM_GCWriteBarrierType writeBarrierType = gc_modron_wrtbar_none;
		MM_GCExtensionsBase* extensions = env->getExtensions();
		if (extensions->isScavengerEnabled()) {
			if (extensions->isConcurrentMarkEnabled()) {
				writeBarrierType = gc_modron_wrtbar_cardmark_and_oldcheck;
			} else {
				writeBarrierType = gc_modron_wrtbar_oldcheck;
			}
		} else if (extensions->isConcurrentMarkEnabled()) {
			writeBarrierType = gc_modron_wrtbar_cardmark;
		}
		return writeBarrierType;
	}

};

#endif /* OMR_GC_MODRON_STANDARD */

#endif /* CONFIGURATIONSTANDARD_HPP_ */
