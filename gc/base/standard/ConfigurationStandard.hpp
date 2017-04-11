/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2017
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

private:
	static const uintptr_t STANDARD_REGION_SIZE_BYTES = 64 * 1024;
	static const uintptr_t STANDARD_ARRAYLET_LEAF_SIZE_BYTES = UDATA_MAX;

	/* Methods */
public:
	virtual MM_GlobalCollector* createGlobalCollector(MM_EnvironmentBase* env);
	virtual MM_Heap* createHeapWithManager(MM_EnvironmentBase* env, uintptr_t heapBytesRequested, MM_HeapRegionManager* regionManager);
	virtual MM_HeapRegionManager* createHeapRegionManager(MM_EnvironmentBase* env);
	virtual J9Pool* createEnvironmentPool(MM_EnvironmentBase* env);

	MM_ConfigurationStandard(MM_EnvironmentBase* env, MM_GCPolicy gcPolicy)
		: MM_Configuration(env, gcPolicy, mm_regionAlignment, STANDARD_REGION_SIZE_BYTES, STANDARD_ARRAYLET_LEAF_SIZE_BYTES, getWriteBarrierType(env), gc_modron_allocation_type_tlh)
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
