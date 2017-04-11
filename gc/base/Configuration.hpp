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
 ******************************************************************************/

#if !defined(CONFIGURATION_HPP_)
#define CONFIGURATION_HPP_

#include "omrcfg.h"
#include "omrgcconsts.h"
#include "omrpool.h"
#include "omrport.h"

#include "BaseVirtual.hpp"
#include "ConfigurationDelegate.hpp"
#include "InitializationParameters.hpp"

class MM_Dispatcher;
class MM_GCExtensionsBase;
class MM_GlobalCollector;
class MM_Heap;
class MM_HeapRegionManager;
class MM_MemorySpace;
class MM_ObjectAllocationInterface;

struct OMR_VM;
struct OMR_VMThread;

/**
 * Abstract class for defining a configuration.
 * New configurations should derive from this and add themselves to the configMap array.
 */
class MM_Configuration : public MM_BaseVirtual
{
/* Data members / types */
public:

protected:
	MM_ConfigurationDelegate _delegate;
	const MM_AlignmentType _alignmentType;			/**< the alignment type must be applied to memory settings */
	const uintptr_t _defaultRegionSize;				/**< default region size, in bytes */
	const uintptr_t _defaultArrayletLeafSize;		/**< default arraylet leaf size, in bytes */
	const MM_GCWriteBarrierType _writeBarrierType;	/**< write barrier to install for GC */
	const MM_GCAllocationType _allocationType;		/**< allocation type */

private:

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

	MM_Configuration(MM_EnvironmentBase* env, MM_GCPolicy gcPolicy, MM_AlignmentType alignmentType, uintptr_t defaultRegionSize, uintptr_t defaultArrayletLeafSize, MM_GCWriteBarrierType writeBarrierType, MM_GCAllocationType allocationType)
		: MM_BaseVirtual()
		, _delegate(gcPolicy)
		, _alignmentType(alignmentType)
		, _defaultRegionSize(defaultRegionSize)
		, _defaultArrayletLeafSize(defaultArrayletLeafSize)
		, _writeBarrierType(writeBarrierType)
		, _allocationType(allocationType)
	{
		_typeId = __FUNCTION__;
	}

protected:
	virtual bool initialize(MM_EnvironmentBase* env);
	virtual void tearDown(MM_EnvironmentBase* env);

	MM_ConfigurationDelegate *getConfigurationDelegate() { return &_delegate; }

	virtual MM_EnvironmentBase* allocateNewEnvironment(MM_GCExtensionsBase* extensions, OMR_VMThread* omrVMThread) = 0;
	virtual bool initializeEnvironment(MM_EnvironmentBase* env);

	/**
	 * Once the region size is calculated each configuration needs to verify that
	 * is is valid.
	 *
	 * @param env[in] - the current environment
	 * @param regionSize[in] - the current regionSize to verify
	 * @return valid - is the regionSize valid
	 */
	virtual bool verifyRegionSize(MM_EnvironmentBase* env, uintptr_t regionSize) { return true; }

	/**
	 * Initializes the NUMAManager.  All overriding implementations should call this.
	 *
	 * @param env[in] - the current environment
	 * @return whether NUMAMAnager was initialized or not.  False implies startup failure.
	 */
	virtual bool initializeNUMAManager(MM_EnvironmentBase* env);

private:
	/**
	 * Sets the number of gc threads
	 *
	 * @param env[in] - the current environment
	 */
	void initializeGCThreadCount(MM_EnvironmentBase* env);

	/**
	 * Sets GC parameters that are dependent on the number of gc threads (if not previously initialized):
	 *
	 * MM_GCExtensionsBase::packetListSplit
	 * MM_GCExtensionsBase::cacheListSplit
	 * MM_GCExtensionsBase::splitFreeListSplitAmount
	 *
	 * @param env[in] - the current environment
	 */
	void initializeGCParameters(MM_EnvironmentBase* env);

	uintptr_t getAlignment(MM_GCExtensionsBase* extensions, MM_AlignmentType type);
	bool initializeRegionSize(MM_EnvironmentBase* env);
	bool initializeArrayletLeafSize(MM_EnvironmentBase* env);
	bool initializeRunTimeObjectAlignmentAndCRShift(MM_EnvironmentBase* env, MM_Heap* heap);
	uintptr_t calculatePowerOfTwoShift(MM_EnvironmentBase* env, uintptr_t value)
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
