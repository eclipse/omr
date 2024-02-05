/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(CONFIGURATION_HPP_)
#define CONFIGURATION_HPP_

#include "omrcfg.h"
#include "omrgcconsts.h"
#include "omrpool.h"
#include "omrport.h"

#include "BaseVirtual.hpp"
#include "CardTable.hpp"
#include "ConfigurationDelegate.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "InitializationParameters.hpp"
#include "Scavenger.hpp"
#include "SlotObject.hpp"

class MM_GCExtensionsBase;
class MM_GlobalCollector;
class MM_Heap;
class MM_HeapRegionManager;
class MM_MemorySpace;
class MM_ObjectAllocationInterface;
class MM_ParallelDispatcher;

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

	/**
	 * Create set of collectors for given configuration.
	 * It might be a Global Collector accompanied with Local Collector if necessary.
	 *
	 * @param env[in] the current thread
	 *
	 * @return Pointer to created Global Collector or NULL
	 */
	virtual MM_GlobalCollector* createCollectors(MM_EnvironmentBase* env) = 0;

	/**
	 * Destroy Garbage Collectors
	 *
	 * @param[in] env the current environment.
	 *
	 * @return void
	 */
	virtual void destroyCollectors(MM_EnvironmentBase* env);

	MM_Heap* createHeap(MM_EnvironmentBase* env, uintptr_t heapBytesRequested);
	virtual MM_Heap* createHeapWithManager(MM_EnvironmentBase* env, uintptr_t heapBytesRequested, MM_HeapRegionManager* regionManager) = 0;
	virtual MM_HeapRegionManager* createHeapRegionManager(MM_EnvironmentBase* env) = 0;
	virtual MM_MemorySpace* createDefaultMemorySpace(MM_EnvironmentBase* env, MM_Heap* heap, MM_InitializationParameters* parameters) = 0;
	MM_EnvironmentBase* createEnvironment(MM_GCExtensionsBase* extensions, OMR_VMThread* vmThread);
	virtual J9Pool* createEnvironmentPool(MM_EnvironmentBase* env) = 0;
	virtual MM_ParallelDispatcher* createParallelDispatcher(MM_EnvironmentBase* env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize);

	bool initializeHeapRegionDescriptor(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* region) { return _delegate.initializeHeapRegionDescriptorExtension(env, region); }
	void teardownHeapRegionDescriptor(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* region) { _delegate.teardownHeapRegionDescriptorExtension(env, region); }

	MMINLINE MM_GCWriteBarrierType getBarrierType() { return _writeBarrierType; }

	MMINLINE bool
	isSnapshotAtTheBeginningBarrierEnabled()
	{
		return (getBarrierType() == gc_modron_wrtbar_satb_and_oldcheck) ||
				(getBarrierType() == gc_modron_wrtbar_satb);
	}

	MMINLINE bool
	isIncrementalUpdateBarrierEnabled()
	{
		return (getBarrierType() == gc_modron_wrtbar_cardmark_and_oldcheck) ||
				(getBarrierType() == gc_modron_wrtbar_cardmark);
	}

	/**
	 * Delegated method to determine when to start tracking heap fragmentation, which should be inhibited
	 * until the heap has grown to a stable operational size.
	 */
	MMINLINE bool canCollectFragmentationStats(MM_EnvironmentBase* env) { return _delegate.canCollectFragmentationStats(env); }

	/**
	 * Called once during startup to indicate that the default memory space has been allocated.
	 * The configuration may use this opportunity to configure values related to the heap address.
	 * 
	 * @param[in] extensions - the extensions associated with the JavaVM
	 * @param[in] defaultMemorySpace - the newly allocated memory space
	 */
	virtual void defaultMemorySpaceAllocated(MM_GCExtensionsBase* extensions, void* defaultMemorySpace);

	virtual void kill(MM_EnvironmentBase* env);

	/* Default number of GC threads based on h/w, os and container limits */
	virtual uintptr_t defaultGCThreadCount(MM_EnvironmentBase* env);

	/**
	 * Sets the number of gc threads
	 *
	 * @param env[in] - the current environment
	 */
	virtual void initializeGCThreadCount(MM_EnvironmentBase* env);

	/**
	 * Initialize GC parameters that are uninitialized and dependent on the number of GC threads:
	 *
	 * MM_GCExtensionsBase::packetListSplit
	 * MM_GCExtensionsBase::cacheListSplit
	 * MM_GCExtensionsBase::splitFreeListSplitAmount
	 *
	 * @param[in] env the current environment
	 */
	void initializeGCParameters(MM_EnvironmentBase* env);

#if defined(J9VM_OPT_CRIU_SUPPORT)
	/**
	 * Update the configuration to reflect the restore environment and parameters.
	 *
	 * The standard configurations are currently the only configurations that support
	 * CRIU and are therefore expected to implement this method.
	 *
	 * @param[in] env the current environment.
	 * @return boolean indicating whether the configuration was successfully updated.
	 */
	virtual bool reinitializeForRestore(MM_EnvironmentBase* env);
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

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
	MM_ConfigurationDelegate *getConfigurationDelegate() { return &_delegate; }

	virtual bool initialize(MM_EnvironmentBase* env);
	virtual void tearDown(MM_EnvironmentBase* env);

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
