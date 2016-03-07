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

/**
 * @file
 * @ingroup GC_Base
 */

#include "omrcfg.h"

#include "Configuration.hpp"

#include "ConfigurationLanguageInterface.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalAllocationManager.hpp"
#include "GlobalCollector.hpp"
#include "Heap.hpp"
#include "HeapRegionManager.hpp"
#include "OMR_VM.hpp"
#include "OMR_VMThread.hpp"
#include "MemoryManager.hpp"
#include "ParallelDispatcher.hpp"
#include "ReferenceChainWalkerMarkMap.hpp"

void
MM_Configuration::kill(MM_EnvironmentBase* env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_Configuration::initialize(MM_EnvironmentBase* env)
{
	bool result = initializeRegionSize(env);
	MM_GCExtensionsBase* extensions = env->getExtensions();
	if (result) {
		result = initializeArrayletLeafSize(env);
		if (result) {
			/* excessivegc is enabled by default */
			if (!extensions->excessiveGCEnabled._wasSpecified) {
				extensions->excessiveGCEnabled._valueSpecified = true;
			}
			initializeWriteBarrierType(env);
			initializeAllocationType(env);
			result = initializeNUMAManager(env);
			if (result) {
				initializeGCThreadCount(env);
			}
		}
	}

	if (result) {
		extensions->_lightweightNonReentrantLockPool = pool_new(sizeof(J9ThreadMonitorTracing), 0, 0, 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_MM, POOL_FOR_PORT(env->getPortLibrary()));
		if (NULL == extensions->_lightweightNonReentrantLockPool) {
			result = false;
		}
	}
	return result;
}

void
MM_Configuration::tearDown(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	/* referenceChainWalkerMarkMap must be destroyed before Memory Manager is killed */
	if (NULL != extensions->referenceChainWalkerMarkMap) {
		extensions->referenceChainWalkerMarkMap->kill(env);
		extensions->referenceChainWalkerMarkMap = NULL;
	}

	MM_Collector *collector = extensions->getGlobalCollector();
	if (NULL != collector) {
		collector->kill(env);
		extensions->setGlobalCollector(NULL);
	}

	if (!extensions->isMetronomeGC()) {
		/* In Metronome, dispatcher is created and destroyed by the collector */
		if (NULL != extensions->dispatcher) {
			extensions->dispatcher->kill(env);
			extensions->dispatcher = NULL;
		}
	}

	if (NULL != extensions->globalAllocationManager) {
		extensions->globalAllocationManager->kill(env);
		extensions->globalAllocationManager = NULL;
	}

	if (NULL != extensions->heap) {
		extensions->heap->kill(env);
		extensions->heap = NULL;
	}	

	if (NULL != extensions->memoryManager) {
		extensions->memoryManager->kill(env);
		extensions->memoryManager = NULL;
	}

	if (NULL != extensions->heapRegionManager) {
		extensions->heapRegionManager->kill(env);
		extensions->heapRegionManager = NULL;
	}

	if (NULL != extensions->_lightweightNonReentrantLockPool) {
		pool_kill(extensions->_lightweightNonReentrantLockPool);
		extensions->_lightweightNonReentrantLockPool = NULL;
	}

	if (NULL != _configurationLanguageInterface) {
		_configurationLanguageInterface->kill(env);
		_configurationLanguageInterface = NULL;
	}

	/*
	 * Clear out the NUMA Manager, too, since we were the first ones to tell it to cache data
	 * Do it last, because some collectors allocate/free resources based on number of NUMA nodes.
	*/
	extensions->_numaManager.shutdownNUMASupport(env);
}

/**
 * Initialize and return the appropriate object allocation interface for the given Environment.
 * Given the allocation scheme and particular environment, select and return the appropriate object allocation interface.
 * @return new subtype instance of MM_ObjectAllocationInterface, or NULL on failure.
 */
MM_ObjectAllocationInterface*
MM_Configuration::createObjectAllocationInterface(MM_EnvironmentBase* env)
{
	return _configurationLanguageInterface->createObjectAllocationInterface(env);
}

/**
 * Initialize and return the appropriate Environment for this Configuration.
 * 
 * @param extensions GCExtensions
 * @param vmThread The thread to create this environment for
 * 
 * @return env The newly allocated env or NULL
 */
MM_EnvironmentBase*
MM_Configuration::createEnvironment(MM_GCExtensionsBase* extensions, OMR_VMThread* omrVMThread)
{
	MM_EnvironmentBase* env = allocateNewEnvironment(extensions, omrVMThread);
	if (NULL == env) {
		return NULL;
	}
	if (!initializeEnvironment(env)) {
		env->kill();
		return NULL;
	}
	return env;
}

/**
 * Initialize the Environment with the appropriate values.
 * @Note if this function is overridden the overridding function
 * has to call super::initializeEnvironment before it does any other work.
 * 
 * @param env The environment to initialize
 */
bool
MM_Configuration::initializeEnvironment(MM_EnvironmentBase* env)
{
	env->_envLanguageInterface = _configurationLanguageInterface->createEnvironmentLanguageInterface(env);
	if (NULL == env->_envLanguageInterface) {
		return false;
	}

	env->_objectAllocationInterface = createObjectAllocationInterface(env);
	if (NULL == env->_objectAllocationInterface) {
		return false;
	}

	if (!_configurationLanguageInterface->initializeEnvironment(env)) {
		return false;
	}

	return true;
}

void
MM_Configuration::defaultMemorySpaceAllocated(MM_GCExtensionsBase* extensions, void* defaultMemorySpace)
{
	/* do nothing */
}

MM_Heap*
MM_Configuration::createHeap(MM_EnvironmentBase* env, uintptr_t heapBytesRequested)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	if (NULL == extensions->memoryManager) {
		extensions->memoryManager = MM_MemoryManager::newInstance(env);
		if (NULL == extensions->memoryManager) {
			return NULL;
		}
	}

	if (NULL == extensions->heapRegionManager) {
		extensions->heapRegionManager = createHeapRegionManager(env);
		if (NULL == extensions->heapRegionManager) {
			return NULL;
		}
	}

	MM_Heap* heap = createHeapWithManager(env, heapBytesRequested, extensions->heapRegionManager);
	if (NULL != heap) {
		if (!heap->initializeHeapRegionManager(env, extensions->heapRegionManager)) {
			heap->kill(env);
			heap = NULL;
		}

		if (!initializeRunTimeObjectAlignmentAndCRShift(env, heap)) {
			heap->kill(env);
			heap = NULL;
		}

		/* VM Design 1869: kill the heap if it was allocated but not in the area requested by the fvtest options and then let it fall through to the normal error handling */
		if ((heap->getHeapBase() < extensions->fvtest_verifyHeapAbove)
		|| ((NULL != extensions->fvtest_verifyHeapBelow) && (heap->getHeapTop() > extensions->fvtest_verifyHeapBelow))
		) {
			heap->kill(env);
			heap = NULL;
		}
	}

	return heap;
}

bool
MM_Configuration::initializeRunTimeObjectAlignmentAndCRShift(MM_EnvironmentBase* env, MM_Heap* heap)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	OMR_VM *omrVM = env->getOmrVM();

#if defined (J9VM_GC_COMPRESSED_POINTERS)
	UDATA heapTop = (uintptr_t)heap->getHeapTop();
	UDATA maxAddressValue = (uintptr_t)1 << 32;
	UDATA shift = (extensions->shouldAllowShiftingCompression) ? LOW_MEMORY_HEAP_CEILING_SHIFT : 0;
	bool canChangeShift = true;

	if (extensions->shouldForceSpecifiedShiftingCompression) {
		shift = extensions->forcedShiftingCompressionAmount;
		canChangeShift = false;
	}
	if (heapTop <= (maxAddressValue << shift)) {
		/* now, try to clamp the shifting */
		if (canChangeShift) {
			intptr_t underShift = (intptr_t)shift - 1;
			while ((heapTop <= (maxAddressValue << underShift)) && (underShift >= 0)) {
				underShift -= 1;
			}
			shift = (uintptr_t)(underShift + 1);
		}
	} else {
		/* impossible geometry:  use an assert for now but just return false once we are done testing the shifting */
		Assert_MM_unreachable();
		return false;
	}
#if !defined(S390) && !defined(J9ZOS390)
	/* s390 benefits from smaller shift values but other platforms don't so just force the shift to 3 if it was not 0 to save
	 * on testing resources.
	 * (still honour the "canChangeShift" flag, though, since we want our testing options to still work)
	 */
	if ((canChangeShift) && (0 != shift) && (shift < DEFAULT_LOW_MEMORY_HEAP_CEILING_SHIFT)) {
		shift = DEFAULT_LOW_MEMORY_HEAP_CEILING_SHIFT;
	}
#endif /* defined(S390) */

	omrVM->_compressedPointersShift = shift;

#endif /* J9VM_GC_COMPRESSED_POINTERS */

#define MINIMUM_OBJECT_ALIGNMENT 0x8
#define MINIMUM_OBJECT_ALIGNMENT_SHIFT 0x3
	uintptr_t objectAlignmentInBytes = OMR_MAX((uintptr_t)1 << omrVM->_compressedPointersShift, MINIMUM_OBJECT_ALIGNMENT);
	uintptr_t objectAlignmenShift = OMR_MAX(omrVM->_compressedPointersShift, MINIMUM_OBJECT_ALIGNMENT_SHIFT);

	extensions->objectModel.setObjectAlignmentInBytes(objectAlignmentInBytes);
	extensions->objectModel.setObjectAlignmentShift(objectAlignmenShift);

	omrVM->_objectAlignmentInBytes = objectAlignmentInBytes;
	omrVM->_objectAlignmentShift = objectAlignmenShift;

	return true;
}

void
MM_Configuration::prepareParameters(OMR_VM* omrVM,
									uintptr_t minimumSpaceSize,
									uintptr_t minimumNewSpaceSize,
									uintptr_t initialNewSpaceSize,
									uintptr_t maximumNewSpaceSize,
									uintptr_t minimumTenureSpaceSize,
									uintptr_t initialTenureSpaceSize,
									uintptr_t maximumTenureSpaceSize,
									uintptr_t memoryMax,
									uintptr_t tenureFlags,
									MM_InitializationParameters* parameters)
{
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(omrVM);
	MM_Heap* heap = extensions->heap;
	uintptr_t alignment = getAlignment(extensions, _alignmentType);

	uintptr_t maximumHeapSize = MM_Math::roundToFloor(alignment, heap->getMaximumMemorySize());

	minimumNewSpaceSize = MM_Math::roundToCeiling(alignment * 2, minimumNewSpaceSize);
	minimumTenureSpaceSize = MM_Math::roundToCeiling(alignment, minimumTenureSpaceSize);

	maximumNewSpaceSize = MM_Math::roundToCeiling(alignment * 2, maximumNewSpaceSize);
	maximumTenureSpaceSize = MM_Math::roundToCeiling(alignment, maximumTenureSpaceSize);

	minimumSpaceSize = OMR_MAX(MM_Math::roundToCeiling(alignment, minimumSpaceSize), minimumNewSpaceSize + minimumTenureSpaceSize);
	memoryMax = OMR_MAX(MM_Math::roundToCeiling(alignment, memoryMax), maximumTenureSpaceSize + maximumNewSpaceSize);

	maximumHeapSize = OMR_MIN(maximumHeapSize, memoryMax);

	initialNewSpaceSize = MM_Math::roundToCeiling(alignment * 2, initialNewSpaceSize);
	initialTenureSpaceSize = MM_Math::roundToCeiling(alignment, initialTenureSpaceSize);

	/* It is possible the heap is smaller than the requested Xmx value, 
	 * ensure none of the options are greater then the maximum heap size
	 */
	parameters->_minimumSpaceSize = OMR_MIN(maximumHeapSize, minimumSpaceSize);
	parameters->_minimumNewSpaceSize = OMR_MIN(maximumHeapSize, minimumNewSpaceSize);
	parameters->_initialNewSpaceSize = OMR_MIN(maximumHeapSize, initialNewSpaceSize);
	parameters->_maximumNewSpaceSize = OMR_MIN(maximumHeapSize, maximumNewSpaceSize);

	parameters->_minimumOldSpaceSize = OMR_MIN(maximumHeapSize - parameters->_minimumNewSpaceSize, minimumTenureSpaceSize);
	parameters->_initialOldSpaceSize = OMR_MIN(maximumHeapSize - parameters->_initialNewSpaceSize, initialTenureSpaceSize);
	parameters->_maximumOldSpaceSize = OMR_MIN(maximumHeapSize, maximumTenureSpaceSize);
	parameters->_maximumSpaceSize = maximumHeapSize;
}

uintptr_t
MM_Configuration::getAlignment(MM_GCExtensionsBase* extensions, MM_AlignmentType type)
{
	uintptr_t result = 0;

	switch (type) {
	case mm_heapAlignment:
		result = extensions->heapAlignment;
		break;

	case mm_regionAlignment:
		result = extensions->regionSize;
		break;
	}

	return result;
}

bool
MM_Configuration::initializeRegionSize(MM_EnvironmentBase* env)
{
	bool result = true;
	MM_GCExtensionsBase* extensions = env->getExtensions();
	uintptr_t regionSize = extensions->regionSize;
	if (0 == regionSize) {
		regionSize = internalGetDefaultRegionSize(env);
	}

	uintptr_t shift = calculatePowerOfTwoShift(env, regionSize);
	if (0 == shift) {
		result = false;
	} else {
		/* set the log and the power of two size */
		uintptr_t powerOfTwoRegionSize = (1 << shift);
		extensions->regionSize = powerOfTwoRegionSize;
		result = verifyRegionSize(env, powerOfTwoRegionSize);
	}

	return result;
}

bool
MM_Configuration::initializeArrayletLeafSize(MM_EnvironmentBase* env)
{
	bool result = true;
	uintptr_t shift = 0;
	uintptr_t arrayletLeafSize = internalGetDefaultArrayletLeafSize(env);
	if (UDATA_MAX != arrayletLeafSize) {
		shift = calculatePowerOfTwoShift(env, arrayletLeafSize);
		if (0 == shift) {
			result = false;
		} else {
			arrayletLeafSize = (uintptr_t)1 << shift;
		}
	}

	if (result) {
		OMR_VM* omrVM = env->getOmrVM();
		omrVM->_arrayletLeafSize = arrayletLeafSize;
		omrVM->_arrayletLeafLogSize = shift;

		_configurationLanguageInterface->initializeArrayletLeafSize(env);
	}

	return result;
}

bool
MM_Configuration::initializeSizeClasses(MM_EnvironmentBase* env)
{
	/* Note: if memory is allocated here (not at present) a teardown method must be implemented to free it */
	bool result = _configurationLanguageInterface->initializeSizeClasses(env);
	return result;
}

void
MM_Configuration::initializeWriteBarrierType(MM_EnvironmentBase* env)
{
	_configurationLanguageInterface->initializeWriteBarrierType(env, internalGetWriteBarrierType(env));
}

void
MM_Configuration::initializeAllocationType(MM_EnvironmentBase* env)
{
	_configurationLanguageInterface->initializeAllocationType(env, internalGetAllocationType(env));
}

void
MM_Configuration::initializeGCThreadCount(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	if (!extensions->gcThreadCountForced) {
		OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
		extensions->gcThreadCount = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_TARGET);

		/* but not higher than maximum default */
		if (_configurationLanguageInterface->getMaxGCThreadCount() < extensions->gcThreadCount) {
			extensions->gcThreadCount = _configurationLanguageInterface->getMaxGCThreadCount();
		}
	}
}

bool
MM_Configuration::initializeNUMAManager(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	bool result = extensions->_numaManager.recacheNUMASupport(env);
	return result;
}

MM_Dispatcher *
MM_Configuration::createDispatcher(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize)
{
	return MM_ParallelDispatcher::newInstance(env, handler, handler_arg, defaultOSStackSize);
}
