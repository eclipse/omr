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

#include "ParallelHeapWalker.hpp"

#include "ModronAssertions.h"

#include "GCExtensionsBase.hpp"
#include "ParallelTask.hpp"
#include "Dispatcher.hpp"
#include "HeapMapIterator.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionManager.hpp"
#include "MarkMap.hpp"
#include "MarkMapSegmentChunkIterator.hpp"
#include "MemorySubSpace.hpp"
#include "ParallelGlobalGC.hpp"
#include "ParallelObjectHeapIterator.hpp"
#include "ObjectModel.hpp"
#include "OMRVMInterface.hpp"

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ParallelObjectDoTask : public MM_ParallelTask
{
	/*
	 * Data members
	 */
private:
	MM_HeapWalkerObjectFunc _function;
	void *_userData;
	UDATA _walkFlags;

	MM_ParallelHeapWalker *_heapWalker;

protected:
public:

	/*
	 * Function members
	 */
public:
	virtual UDATA getVMStateID() { return J9VMSTATE_GC_PARALLEL_OBJECT_DO; };

	virtual void run(MM_EnvironmentBase *env);

	/*
	 * Create a ParallelObjectAndVMSlotsDoTask object.
	 */
	MM_ParallelObjectDoTask(MM_EnvironmentBase *env, MM_ParallelHeapWalker *heapWalker, MM_HeapWalkerObjectFunc function, void *userData, UDATA walkFlags, bool parallel)
		: MM_ParallelTask(env, env->getExtensions()->dispatcher)
		, _function(function)
		, _userData(userData)
		, _walkFlags(walkFlags)
		, _heapWalker(heapWalker)
	{
		_typeId = __FUNCTION__;
	}
};

/**
 * newInstance of Parallel Heap Walker
 */
MM_ParallelHeapWalker *
MM_ParallelHeapWalker::newInstance(MM_ParallelGlobalGC *globalCollector, MM_MarkMap *markMap, MM_EnvironmentBase *env)
{
	MM_ParallelHeapWalker *heapWalker;

	heapWalker = (MM_ParallelHeapWalker *)env->getForge()->allocate(sizeof(MM_ParallelHeapWalker), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (heapWalker) {
		new(heapWalker) MM_ParallelHeapWalker(globalCollector, markMap);
	}

	return heapWalker;
}

/**
 * Walk through all live objects of the heap in parallel and apply the provided function.
 */
void
MM_ParallelHeapWalker::allObjectsDoParallel(MM_EnvironmentBase *env, MM_HeapWalkerObjectFunc function, void *userData, UDATA walkFlags)
{
	Trc_MM_ParallelHeapWalker_allObjectsDoParallel_Entry(env->getLanguageVMThread());
	MM_GCExtensionsBase *extensions = env->getExtensions();

	/* determine the size of the segment chunks to use for parallel walks */
	UDATA threadCount = env->_currentTask->getThreadCount();
	UDATA heapChunkFactor = 1;
	if ((threadCount > 1) && _markMap->isMarkMapValid()) {
		heapChunkFactor = threadCount * 8;
	}
	UDATA parallelChunkSize = extensions->heap->getMemorySize() / heapChunkFactor;
	parallelChunkSize = MM_Math::roundToCeiling(extensions->heapAlignment, parallelChunkSize);

	/* Perform the parallel object heap iteration */
	UDATA objectsWalked = 0;
	MM_Heap *heap = extensions->heap;
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	regionManager->lock();
	GC_HeapRegionIterator regionIterator(regionManager);
	MM_HeapRegionDescriptor *region = NULL;
	OMR_VMThread *omrVMThread = env->getOmrVMThread();

	while (NULL != (region = regionIterator.nextRegion())) {
		GC_ParallelObjectHeapIterator objectHeapIterator(env, region, region->getLowAddress(), region->getHighAddress(), _markMap, parallelChunkSize);
		omrobjectptr_t object = NULL;
		while ((object = objectHeapIterator.nextObject()) != NULL) {
			function(omrVMThread, region, object, userData);
			objectsWalked += 1;
		}
	}
	regionManager->unlock();
	Trc_MM_ParallelHeapWalker_allObjectsDoParallel_Exit(env->getLanguageVMThread(), heapChunkFactor, parallelChunkSize, objectsWalked);
}

/**
 * Walk through all live objects of the heap and apply the provided function.
 * If parallel is set to true, task is dispatched to GC threads and walks the heap segments in parallel,
 * otherwise walk all objects in the heap in a single threaded linear fashion.
 */
void
MM_ParallelHeapWalker::allObjectsDo(MM_EnvironmentBase *env, MM_HeapWalkerObjectFunc function, void *userData, UDATA walkFlags, bool parallel, bool prepareHeapForWalk)
{
	if (parallel) {
		GC_OMRVMInterface::flushCachesForWalk(env->getOmrVM());
		if (prepareHeapForWalk) {
			_globalCollector->prepareHeapForWalk(env);
		}

		MM_ParallelObjectDoTask objectDoTask(env, this, function, userData, walkFlags, parallel);
		env->getExtensions()->dispatcher->run(env, &objectDoTask);
	} else {
		MM_HeapWalker::allObjectsDo(env, function, userData, walkFlags, parallel, prepareHeapForWalk);
	}
}

/**
 * gets the heap walker and calls the actual objectSlotsDo function
 */
void
MM_ParallelObjectDoTask::run(MM_EnvironmentBase *env)
{
	_heapWalker->allObjectsDoParallel(env, _function, _userData, _walkFlags);
}
