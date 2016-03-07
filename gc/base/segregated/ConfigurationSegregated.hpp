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
 * @ingroup GC_Modron_Metronome
 */

#if !defined(CONFIGURATIONSEGREGATED_HPP_)
#define CONFIGURATIONSEGREGATED_HPP_

#include "omrcfg.h"

#include "Configuration.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_ConfigurationLanguageInterface;
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

	/*
	 * Function members
	 */
public:
	static MM_Configuration *newInstance(MM_EnvironmentBase *env, MM_ConfigurationLanguageInterface *cli);

	virtual MM_GlobalCollector *createGlobalCollector(MM_EnvironmentBase *env);
	virtual MM_Heap *createHeapWithManager(MM_EnvironmentBase *env, uintptr_t heapBytesRequested, MM_HeapRegionManager *regionManager);
	virtual MM_HeapRegionManager *createHeapRegionManager(MM_EnvironmentBase *env);
	virtual MM_MemorySpace *createDefaultMemorySpace(MM_EnvironmentBase *env, MM_Heap *heap, MM_InitializationParameters *parameters);
	virtual J9Pool *createEnvironmentPool(MM_EnvironmentBase *env);
	virtual MM_ObjectAllocationInterface *createObjectAllocationInterface(MM_EnvironmentBase *env);
	virtual MM_SegregatedAllocationTracker* createAllocationTracker(MM_EnvironmentBase* env);
	virtual MM_Dispatcher *createDispatcher(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize);
	
	virtual void defaultMemorySpaceAllocated(MM_GCExtensionsBase *extensions, void* defaultMemorySpace);
	
	MM_ConfigurationSegregated(MM_EnvironmentBase *env, MM_ConfigurationLanguageInterface* configurationLanguageInterface) :
		MM_Configuration(env, configurationLanguageInterface, mm_regionAlignment)
	{
		_typeId = __FUNCTION__;
	};

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	virtual MM_EnvironmentBase *allocateNewEnvironment(MM_GCExtensionsBase *extensions, OMR_VMThread *omrVMThread);
	virtual bool initializeEnvironment(MM_EnvironmentBase *env);

	/**
	 * Each configuration is responsible for providing a default region size.
	 * This size will be corrected to ensure that is a power of 2.
	 *
	 * @return regionSize[in] - The default region size for this configuration
	 */
	virtual uintptr_t internalGetDefaultRegionSize(MM_EnvironmentBase *env);

	/**
	 * Once the region size is calculated each configuration needs to verify that
	 * is is valid.
	 *
	 * @param env[in] - the current environment
	 * @param regionSize[in] - the current regionSize to verify
	 * @return valid - is the regionSize valid
	 */
	virtual bool verifyRegionSize(MM_EnvironmentBase *env, uintptr_t regionSize) {return true;}

	/**
	 * Each configuration is responsible for providing a default arrayletLeafSize.
	 * This size will be corrected to ensure that is a power of 2.
	 *
	 * @return regionSize- The default region size for this configuration
	 */
	virtual uintptr_t internalGetDefaultArrayletLeafSize(MM_EnvironmentBase *env);

	/**
	 * Each configuration is responsible for providing the barrier type it is using
	 *
	 * @return barrierType - The barrier type being used for this configuration
	 */
	virtual uintptr_t internalGetWriteBarrierType(MM_EnvironmentBase *env);

	/**
	 * Each configuration is responsible for providing the allocation type it is using
	 *
	 * @return allocationType - The allocation type being used for this configuration
	 */
	virtual uintptr_t internalGetAllocationType(MM_EnvironmentBase *env);

private:
	bool aquireAllocationContext(MM_EnvironmentBase *env);
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* CONFIGURATIONSEGREGATED_HPP_ */
