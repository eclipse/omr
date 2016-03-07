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
	/* Methods */
public:
	virtual MM_GlobalCollector* createGlobalCollector(MM_EnvironmentBase* env);
	virtual MM_Heap* createHeapWithManager(MM_EnvironmentBase* env, uintptr_t heapBytesRequested, MM_HeapRegionManager* regionManager);
	virtual MM_HeapRegionManager* createHeapRegionManager(MM_EnvironmentBase* env);
	virtual J9Pool* createEnvironmentPool(MM_EnvironmentBase* env);

	MM_ConfigurationStandard(MM_EnvironmentBase* env, MM_ConfigurationLanguageInterface* configurationLanguageInterface)
		: MM_Configuration(env, configurationLanguageInterface, mm_regionAlignment)
	{
		_typeId = __FUNCTION__;
	};

protected:
	virtual bool initialize(MM_EnvironmentBase* env);
	virtual MM_EnvironmentBase* allocateNewEnvironment(MM_GCExtensionsBase* extensions, OMR_VMThread* omrVMThread);

	/**
	 * Each configuration is responsible for providing a default region size.
	 * This size will be corrected to ensure that is a power of 2.
	 *
	 * @return regionSize[in] - The default region size for this configuration
	 */
	virtual uintptr_t internalGetDefaultRegionSize(MM_EnvironmentBase* env);
	/**
	 * Once the region size is calculated each configuration needs to verify that
	 * is is valid.
	 *
	 * @param env[in] - the current environment
	 * @param regionSize[in] - the current regionSize to verify
	 * @return valid - is the regionSize valid
	 */
	virtual bool verifyRegionSize(MM_EnvironmentBase* env, uintptr_t regionSize)
	{
		return true;
	}
	/**
	 * Each configuration is responsible for providing a default arrayletLeafSize.
	 * This size will be corrected to ensure that is a power of 2.
	 *
	 * @return regionSize - The default region size for this configuration
	 */
	virtual uintptr_t internalGetDefaultArrayletLeafSize(MM_EnvironmentBase* env);
	/**
	 * Each configuration is responsible for providing the barrier type it is using
	 *
	 * @return barrierType - The barrier type being used for this configuration
	 */
	virtual uintptr_t internalGetWriteBarrierType(MM_EnvironmentBase* env);
	/**
	 * Each configuration is responsible for providing the allocation type it is using
	 *
	 * @return allocationType - The allocation type being used for this configuration
	 */
	virtual uintptr_t internalGetAllocationType(MM_EnvironmentBase* env);

private:
};

#endif /* OMR_GC_MODRON_STANDARD */

#endif /* CONFIGURATIONSTANDARD_HPP_ */
