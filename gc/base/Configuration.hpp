/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#if !defined(CONFIGURATION_HPP_)
#define CONFIGURATION_HPP_

#include "omrcfg.h"
#include "omrpool.h"
#include "omrport.h"

#include "BaseVirtual.hpp"
#include "ConfigurationLanguageInterface.hpp"
#include "InitializationParameters.hpp"

class MM_ArrayAllocationModel;
class MM_Dispatcher;
class MM_EnvironmentLanguageInterface;
class MM_EnvironmentBase;
class MM_EnvironmentLanguageInterface;
class MM_GCExtensionsBase;
class MM_GlobalCollector;
class MM_Heap;
class MM_HeapRegionManager;
class MM_MemorySpace;
class MM_MixedObjectAllocationModel;
class MM_ObjectAllocationInterface;

struct OMR_VM;
struct OMR_VMThread;

/*
 * list of available GC policies
 */
enum MM_GCPolicy {
	gc_policy_undefined = 0,
	gc_policy_optthruput,
	gc_policy_optavgpause,
	gc_policy_gencon,
	gc_policy_metronome,
	gc_policy_balanced
};

enum MM_AlignmentType {
	mm_heapAlignment = 1,
	mm_regionAlignment
};

class MM_ConfigurationOptions : public MM_BaseNonVirtual {
private:
public:
	MM_GCPolicy _gcPolicy; /**< gc policy (default or configured) */

	bool _forceOptionScavenge; /**< true if Scavenge option is forced in command line */
	bool _forceOptionConcurrentMark; /**< true if Concurrent Mark option is forced in command line */
	bool _forceOptionConcurrentSweep; /**< true if Concurrent Sweep option is forced in command line */
	bool _forceOptionLargeObjectArea; /**< true if Large Object Area option is forced in command line */

	MM_ConfigurationOptions()
		: MM_BaseNonVirtual()
		, _gcPolicy(gc_policy_undefined)
		, _forceOptionScavenge(false)
		, _forceOptionConcurrentMark(false)
		, _forceOptionConcurrentSweep(false)
		, _forceOptionLargeObjectArea(false)
	{
		_typeId = __FUNCTION__;
	}
};

/**
 * Abstract class for defining a configuration.
 * New configurations should derive from this and add themselves to the configMap array.
 */
class MM_Configuration : public MM_BaseVirtual {
/* Data members / types */
public:
protected:
	MM_ConfigurationLanguageInterface* _configurationLanguageInterface;
private:
	const MM_AlignmentType _alignmentType; /**< the alignment type must be applied to memory settings */

/* Methods */
public:
	virtual void prepareParameters(OMR_VM* omrVM,
								   uintptr_t minimumSpaceSize,
								   uintptr_t minimumNewSpaceSize,
								   uintptr_t initialNewSpaceSize,
								   uintptr_t maximumNewSpaceSize,
								   uintptr_t minimumTenureSpaceSize,
								   uintptr_t initialTenureSpaceSize,
								   uintptr_t maximumTenureSpaceSize,
								   uintptr_t memoryMax,
								   uintptr_t tenureFlags,
								   MM_InitializationParameters* parameters);

	virtual MM_GlobalCollector* createGlobalCollector(MM_EnvironmentBase* env) = 0;
	MM_Heap* createHeap(MM_EnvironmentBase* env, uintptr_t heapBytesRequested);
	virtual MM_Heap* createHeapWithManager(MM_EnvironmentBase* env, uintptr_t heapBytesRequested, MM_HeapRegionManager* regionManager) = 0;
	virtual MM_HeapRegionManager* createHeapRegionManager(MM_EnvironmentBase* env) = 0;
	virtual MM_MemorySpace* createDefaultMemorySpace(MM_EnvironmentBase* env, MM_Heap* heap, MM_InitializationParameters* parameters) = 0;
	MM_EnvironmentBase* createEnvironment(MM_GCExtensionsBase* extensions, OMR_VMThread* vmThread);
	virtual J9Pool* createEnvironmentPool(MM_EnvironmentBase* env) = 0;
	virtual MM_Dispatcher *createDispatcher(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize);

	/**
	 * Called once during startup to indicate that the default memory space has been allocated.
	 * The configuration may use this opportunity to configure values related to the heap address.
	 * 
	 * @param[in] extensions - the extensions associated with the JavaVM
	 * @param[in] defaultMemorySpace - the newly allocated memory space
	 */
	virtual void defaultMemorySpaceAllocated(MM_GCExtensionsBase* extensions, void* defaultMemorySpace);

	virtual void kill(MM_EnvironmentBase* env);

	MM_Configuration(MM_EnvironmentBase* env, MM_ConfigurationLanguageInterface* configurationLanguageInterface, MM_AlignmentType alignmentType)
		: MM_BaseVirtual()
		, _configurationLanguageInterface(configurationLanguageInterface)
		, _alignmentType(alignmentType)
	{
		_typeId = __FUNCTION__;
	}

protected:
	virtual bool initialize(MM_EnvironmentBase* env);
	virtual void tearDown(MM_EnvironmentBase* env);
	virtual MM_EnvironmentBase* allocateNewEnvironment(MM_GCExtensionsBase* extensions, OMR_VMThread* omrVMThread) = 0;
	virtual bool initializeEnvironment(MM_EnvironmentBase* env);
	virtual MM_ObjectAllocationInterface* createObjectAllocationInterface(MM_EnvironmentBase* env);

	/**
	 * Sets the number of gc threads
	 *
	 * @param env[in] - the current environment
	 */
	void initializeGCThreadCount(MM_EnvironmentBase* env);
	/**
	 * Initialize OMR_VM->_sizeClasses using J9JavaVM->realtimeSizeClasses
	 *
	 * @param env[in] - the current environment
	 */
	bool initializeSizeClasses(MM_EnvironmentBase* env);
	/**
	 * Each configuration is responsible for providing a default region size.
	 * This size will be corrected to ensure that is a power of 2.
	 */
	virtual uintptr_t internalGetDefaultRegionSize(MM_EnvironmentBase* env) = 0;
	/**
	 * Once the region size is calculated each configuration needs to verify that
	 * is is valid.
	 *
	 * @param env[in] - the current environment
	 * @param regionSize[in] - the current regionSize to verify
	 * @return valid - is the regionSize valid
	 */
	virtual bool verifyRegionSize(MM_EnvironmentBase* env, uintptr_t regionSize) = 0;
	/**
	 * Each configuration is responsible for providing a default arrayletLeafSize.
	 * This size will be corrected to ensure that is a power of 2.
	 *
	 * @return regionSize - The default region size for this configuration
	 */
	virtual uintptr_t internalGetDefaultArrayletLeafSize(MM_EnvironmentBase* env) = 0;
	/**
	 * Each configuration is responsible for providing the barrier type it is using
	 *
	 * @return barrierType - The barrier type being used for this configuration
	 */
	virtual uintptr_t internalGetWriteBarrierType(MM_EnvironmentBase* env) = 0;
	/**
	 * Each configuration is responsible for providing the allocation type it is using
	 *
	 * @return allocationType - The allocation type being used for this configuration
	 */
	virtual uintptr_t internalGetAllocationType(MM_EnvironmentBase* env) = 0;

	/**
	 * Initializes the NUMAManager.  All overriders should call this.
	 *
	 * OMRTODO move this out of configuration as we should not have to "configure" NUMA.  The only reason this is here is
	 * because ConfigurationIncrementalGenerational.hpp will disable physical NUMA if it would create too many ACs and ideal AC calculation
	 * requires configuration to be done (regionSize set up).
	 *
	 * @param env[in] - the current environment
	 * @return whether NUMAMAnager was initialized or not.  False implies startup failure.
	 */
	virtual bool initializeNUMAManager(MM_EnvironmentBase* env);

private:
	uintptr_t getAlignment(MM_GCExtensionsBase* extensions, MM_AlignmentType type);
	bool initializeRegionSize(MM_EnvironmentBase* env);
	bool initializeArrayletLeafSize(MM_EnvironmentBase* env);
	void initializeWriteBarrierType(MM_EnvironmentBase* env);
	void initializeAllocationType(MM_EnvironmentBase* env);
	bool initializeRunTimeObjectAlignmentAndCRShift(MM_EnvironmentBase* env, MM_Heap* heap);
	MMINLINE uintptr_t calculatePowerOfTwoShift(MM_EnvironmentBase* env, uintptr_t value)
	{
		/* Make sure that the regionSize is a power of two */
		uintptr_t shift = (sizeof(uintptr_t) * 8) - 1;
		while ((shift > 0) && (1 != (value >> shift))) {
			shift -= 1;
		}
		return shift;
	}
};

#endif /* CONFIGURATION_HPP_ */
