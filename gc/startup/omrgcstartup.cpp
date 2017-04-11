/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2017
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

#include "StartupManager.hpp"
#include "omr.h"
#include "mminitcore.h"
#include "objectdescription.h"

#include "AllocateDescription.hpp"
#include "AtomicOperations.hpp"
#include "Collector.hpp"
#include "CollectorLanguageInterface.hpp"
#include "ConfigurationFlat.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "Heap.hpp"
#include "HeapMemorySubSpaceIterator.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionDescriptor.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "ModronAssertions.h"
#include "ObjectAllocationInterface.hpp"
#include "ObjectModel.hpp"
#include "ParallelDispatcher.hpp"
#include "VerboseManager.hpp"

/* ****************
 * Internal helpers
 * ****************/

static omr_error_t
collectorCreationHelper(OMR_VM *omrVM, MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);
	MM_Collector *globalCollector = extensions->configuration->createGlobalCollector(env);
	omr_error_t rc = OMR_ERROR_NONE;

	if (NULL == globalCollector) {
		omrtty_printf("Failed to create global collector.\n");
		rc = OMR_ERROR_INTERNAL;
	} else {
		globalCollector->setGlobalCollector(true);
		extensions->setGlobalCollector(globalCollector);

		if (!extensions->getGlobalCollector()->collectorStartup(extensions)) {
			omrtty_printf("Failed to start global collector.\n");
			rc = OMR_ERROR_INTERNAL;
		}
	}

	return rc;
}

static omr_error_t
heapCreationHelper(OMR_VM *omrVM, MM_StartupManager *startupManager, bool createCollector)
{
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);
	MM_MemorySpace *memorySpace = NULL;
	MM_InitializationParameters parameters;
	omr_error_t rc = OMR_ERROR_NONE;

	/* Create GC Extensions and populate with defaults */
	gcOmrInitializeDefaults(omrVM);
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);
	extensions->_lazyCollectorInit = !createCollector; /* Delay initialize of sweep pool manager if needed. */

	/* The 'fake' environment is necessary because obtaining an environment for a thread
	 * is done by calling out to MM_Configuration, which itself uses the environment
	 * to obtain pointers to MM_GCExtensionsBase and MM_Forge, etc. While this circular
	 * dependency could be eliminated, it would require changes to many interfaces. */
	MM_EnvironmentBase envBase(omrVM);

	/* Set runtime options on extensions */
	if (NULL == startupManager || !startupManager->loadGcOptions(extensions)) {
		omrtty_printf("Failed to load GC startup options.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	extensions->configuration = startupManager->createConfiguration(&envBase);
	if (NULL == extensions->configuration) {
		omrtty_printf("Failed to create configuration.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	extensions->collectorLanguageInterface = startupManager->createCollectorLanguageInterface(&envBase);
	if (NULL == extensions->collectorLanguageInterface) {
		omrtty_printf("Failed to create collector language interface.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	extensions->heap = extensions->configuration->createHeap(&envBase, extensions->memoryMax);
	if (NULL == extensions->heap) {
		omrtty_printf("Failed to create heap.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	/* It is necessary to have a dispatcher up before the collector can be initialized,
	 * since the number of sweep chunks is determined by the number of threads. */
	extensions->dispatcher = extensions->configuration->createDispatcher(&envBase, NULL, NULL, OMR_OS_STACK_SIZE);
	if (NULL == extensions->dispatcher) {
		omrtty_printf("Failed to create dispatcher.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	/* Create the environments pool */
	extensions->environments = extensions->configuration->createEnvironmentPool(&envBase);
	if (NULL == extensions->environments) {
		omrtty_printf("Failed to create environment pool.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	/* Initialize statistic locks */
	if (0 != omrthread_monitor_init_with_name(&extensions->gcStatsMutex, 0, "MM_GCExtensions::gcStats")) {
		omrtty_printf("Failed to create GC statistics mutex.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	extensions->configuration->prepareParameters(omrVM, extensions->initialMemorySize,
		extensions->minNewSpaceSize, extensions->newSpaceSize, extensions->maxNewSpaceSize,
		extensions->minOldSpaceSize, extensions->oldSpaceSize, extensions->maxOldSpaceSize,
		extensions->maxSizeDefaultMemorySpace, MEMORY_TYPE_DISCARDABLE, &parameters);

	if (0 != omrthread_monitor_init_with_name(&extensions->gcExclusiveAccessMutex, 0, "GCExtensions::gcExclusiveAccessMutex")) {
		omrtty_printf("Failed to create gcExclusiveAccessMutex.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	if (0 != omrthread_monitor_init_with_name(&extensions->_lightweightNonReentrantLockPoolMutex, 0, "GCExtensions::_lightweightNonReentrantLockPoolMutex")) {
		omrtty_printf("Failed to create _lightweightNonReentrantLockPoolMutex.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	if (createCollector && (OMR_ERROR_NONE != collectorCreationHelper(omrVM, &envBase))) {
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	memorySpace = extensions->configuration->createDefaultMemorySpace(&envBase, extensions->heap, &parameters);

	if (NULL == memorySpace) {
		omrtty_printf("Failed to create default memory space.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	memorySpace->inflate(&envBase);

	extensions->configuration->defaultMemorySpaceAllocated(extensions, memorySpace);
	extensions->heap->setDefaultMemorySpace(memorySpace);

	if (startupManager->isVerboseEnabled()) {
		extensions->verboseGCManager = startupManager->createVerboseManager(&envBase);
		if (NULL == extensions->verboseGCManager) {
			omrtty_printf("Failed to create verbose GC manager.\n");
			rc = OMR_ERROR_INTERNAL;
			goto done;
		}
		extensions->verboseGCManager->configureVerboseGC(omrVM, startupManager->getVerboseFileName(), 1, 0);
		extensions->verboseGCManager->enableVerboseGC();
		extensions->verboseGCManager->setInitializedTime(omrtime_hires_clock());
	}

done:
	return rc;
}

/* ****************
 *    Public API
 * ****************/

omr_error_t
OMR_GC_InitializeHeap(OMR_VM *omrVM, MM_StartupManager *manager)
{
	return heapCreationHelper(omrVM, manager, false);
}

omr_error_t
OMR_GC_IntializeHeapAndCollector(OMR_VM* omrVM, MM_StartupManager *manager)
{
	return heapCreationHelper(omrVM, manager, true);
}

omr_error_t
OMR_GC_InitializeDispatcherThreads(OMR_VMThread* omrVMThread)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	omr_error_t rc = OMR_ERROR_NONE;

	if (!extensions->dispatcher->startUpThreads()) {
		extensions->dispatcher->shutDownThreads();
		rc = OMR_ERROR_INTERNAL;
	}

	return rc;
}

omr_error_t
OMR_GC_ShutdownDispatcherThreads(OMR_VMThread* omrVMThread)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	omr_error_t rc = OMR_ERROR_NONE;

	if (NULL != extensions->dispatcher) {
		extensions->dispatcher->shutDownThreads();
		extensions->dispatcher->kill(MM_EnvironmentBase::getEnvironment(omrVMThread));
		extensions->dispatcher = NULL;
	}

	return rc;
}

omr_error_t
OMR_GC_InitializeCollector(OMR_VMThread* omrVMThread)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	omr_error_t rc = OMR_ERROR_NONE;

	if (OMR_ERROR_NONE != collectorCreationHelper(omrVMThread->_vm, env)) {
		rc = OMR_ERROR_INTERNAL;
	} else {
		MM_Collector *globalCollector = extensions->getGlobalCollector();
		MM_Heap *heap = env->getMemorySpace()->getHeap();

		/* We now need to inform the existing subspaces about the new collector */
		MM_HeapMemorySubSpaceIterator iter(heap);
		MM_MemorySubSpace *subspace = NULL;
		while (NULL != (subspace = iter.nextSubSpace())) {
			subspace->setCollector(globalCollector);
			MM_MemoryPool *pool = subspace->getMemoryPool();
			/* Setup sweep pool manager which requires a collector */
			if (NULL != pool && !pool->initializeSweepPool(env)) {
				rc = OMR_ERROR_INTERNAL;
				break;
			}
		}

		if (OMR_ERROR_NONE == rc) {
			/* We also need to commit the mark map for the consumed heap */
			MM_HeapRegionDescriptor *region = NULL;
			MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
			GC_HeapRegionIterator regionIterator(regionManager);
			while(NULL != (region = regionIterator.nextRegion())) {
				if (region->isCommitted()) {
					globalCollector->heapAddRange(env,
							env->getMemorySpace()->getDefaultMemorySubSpace(),
							region->getSize(),
							region->getLowAddress(),
							region->getHighAddress());
				}
			}

			/* Make sure sweep scheme is up-to-date with the heap configuration */
			globalCollector->heapReconfigured(env);
		}
	}

	return rc;
}

omr_error_t
OMR_GC_ShutdownCollector(OMR_VMThread* omrVMThread)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	MM_Collector *globalCollector = extensions->getGlobalCollector();
	
	if (NULL != globalCollector) {
		globalCollector->collectorShutdown(extensions);
	}

	return OMR_ERROR_NONE;
}

omr_error_t
OMR_GC_ShutdownHeap(OMR_VM *omrVM)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);
	MM_EnvironmentBase env(omrVM);
	omrthread_t self = NULL;

	if (NULL == extensions) {
		return OMR_ERROR_NONE;
	}

	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		if (NULL != extensions->verboseGCManager) {
			extensions->verboseGCManager->closeStreams(&env);
			extensions->verboseGCManager->disableVerboseGC();
			extensions->verboseGCManager->kill(&env);
			extensions->verboseGCManager = NULL;
		}

		if ((NULL != extensions) && (NULL != extensions->heap)) {
			MM_MemorySpace *defaultSpace = extensions->heap->getDefaultMemorySpace();
			if (NULL != defaultSpace) {
				defaultSpace->kill(&env);
				extensions->heap->setDefaultMemorySpace(NULL);
			}
		}

		if (NULL != extensions->configuration) {
			extensions->configuration->kill(&env);
		}

		extensions->kill(&env);
		omrVM->_gcOmrVMExtensions = NULL;

		omrthread_detach(self);
	}

	return OMR_ERROR_NONE;
}

