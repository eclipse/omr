/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "HeapWalker.hpp"

#include "omrcfg.h"
#include "omrgcconsts.h"

#include "CollectorLanguageInterface.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionManager.hpp"
#include "MemorySubSpace.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"
#include "ObjectIterator.hpp"
#include "ObjectModel.hpp"
#include "OMRVMInterface.hpp"
#include "ParallelDispatcher.hpp"
#include "SlotObject.hpp"
#include "SublistIterator.hpp"
#include "SublistSlotIterator.hpp"
#include "Task.hpp"

/**
 * Auxilary structure too pass both function and userData as one param to heap walker
 */
struct SlotObjectDoUserData {
	MM_HeapWalkerSlotFunc function;
	void *userData;
	uintptr_t oSlotWalkFlag;
};

/**
 * apply the user function to object field
 */
static void
heapWalkerObjectFieldSlotDo(OMR_VM *omrVM, omrobjectptr_t object, GC_SlotObject *slotObject, MM_HeapWalkerSlotFunc oSlotIterator, void *localUserData)
{
	/* decode the field value into an object pointer */
	omrobjectptr_t fieldValue = slotObject->readReferenceFromSlot();

	(*oSlotIterator)(omrVM, &fieldValue, localUserData, 0);

	/* write the value back into the field in case the callback changed it */
	slotObject->writeReferenceToSlot(fieldValue);
}


/**
 * walk through slots of mixed object and apply the user function.
 */
static void
heapWalkerObjectSlotDo(OMR_VM *omrVM, omrobjectptr_t object, MM_HeapWalkerSlotFunc oSlotIterator, void *localUserData)
{
	GC_ObjectIterator objectIterator(omrVM, object);
	GC_SlotObject *slotObject;

	while ((slotObject = objectIterator.nextSlot()) != NULL) {
		heapWalkerObjectFieldSlotDo(omrVM, object, slotObject, oSlotIterator, localUserData);
	}
}

/**
 * walk through slots of an object and apply the user function.
 */
static void
heapWalkerObjectSlotDo(OMR_VMThread *omrVMThread, MM_HeapRegionDescriptor *region, omrobjectptr_t object, void *userData)
{
	OMR_VM *omrVM = omrVMThread->_vm;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);
	MM_HeapWalkerSlotFunc oSlotIterator = (MM_HeapWalkerSlotFunc)((SlotObjectDoUserData *)userData)->function;
	void *localUserData = ((SlotObjectDoUserData *)userData)->userData;


	omrobjectptr_t indirectObject = extensions->objectModel.getIndirectObject(object);
	if (NULL != indirectObject) {
		(*oSlotIterator)(omrVM, &indirectObject, localUserData, 0);
	}

	heapWalkerObjectSlotDo(omrVM, object, oSlotIterator, localUserData);
}

MM_HeapWalker *
MM_HeapWalker::newInstance(MM_EnvironmentBase *env)
{
	MM_HeapWalker *heapWalker;

	heapWalker = (MM_HeapWalker *)env->getForge()->allocate(sizeof(MM_HeapWalker), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (heapWalker) {
		new(heapWalker) MM_HeapWalker();
	}

	return heapWalker;
}

bool
MM_HeapWalker::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Free the receivers memory and all associated resources.
 */
void
MM_HeapWalker::kill(MM_EnvironmentBase *env)
{
	env->getForge()->free(this);
}

/**
 * Walk all slots of objects in the remembered set and apply users function
 */
#if defined(OMR_GC_MODRON_SCAVENGER)
void
MM_HeapWalker::rememberedObjectSlotsDo(MM_EnvironmentBase *env, MM_HeapWalkerSlotFunc function, void *userData, uintptr_t walkFlags, bool parallel)
{
	SlotObjectDoUserData slotObjectDoUserData = { function, userData, walkFlags };
	omrobjectptr_t* slotPtr = NULL;
	MM_SublistPuddle *puddle = NULL;
	OMR_VMThread *omrVMThread = env->getOmrVMThread();

	GC_SublistIterator remSetIterator(&(env->getExtensions()->rememberedSet));
	while ((puddle = remSetIterator.nextList()) != NULL) {
		if (!parallel || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			GC_SublistSlotIterator remSetSlotIterator(puddle);
			while ((slotPtr = (omrobjectptr_t*)remSetSlotIterator.nextSlot()) != NULL) {
				if (*slotPtr != NULL) {
					heapWalkerObjectSlotDo(omrVMThread, NULL, *slotPtr, &slotObjectDoUserData);
				}
			}
		}
	}
}
#endif /* OMR_GC_MODRON_SCAVENGER */

/**
 * Walk all object slots and apply users function
 * Object walker is virtual and specific to the Global Collector
 */
void
MM_HeapWalker::allObjectSlotsDo(MM_EnvironmentBase *env, MM_HeapWalkerSlotFunc function, void *userData, uintptr_t walkFlags, bool parallel, bool prepareHeapForWalk)
{
	SlotObjectDoUserData slotObjectDoUserData = { function, userData, walkFlags };
	uintptr_t modifiedWalkFlags = walkFlags;

#if defined(OMR_GC_MODRON_SCAVENGER)
	/* If J9_MU_WALK_NEW_AND_REMEMBERED_ONLY is specified, and rsOverflow has
	 * occurred, any object in old space might be remembered, so we must walk them all
	 */
	if (env->getExtensions()->isRememberedSetInOverflowState()) {
		modifiedWalkFlags &= ~J9_MU_WALK_NEW_AND_REMEMBERED_ONLY;
	}
#endif /* OMR_GC_MODRON_SCAVENGER */

	allObjectsDo(env, heapWalkerObjectSlotDo, (void *)&slotObjectDoUserData, modifiedWalkFlags, parallel, prepareHeapForWalk);

#if defined(OMR_GC_MODRON_SCAVENGER)
	/* If J9_MU_WALK_NEW_AND_REMEMBERED_ONLY is specified, allObjectsDo will only walk
	 * new spaces, so we must also go through all slots of objects in the remembered set.
	 */
	if (modifiedWalkFlags & J9_MU_WALK_NEW_AND_REMEMBERED_ONLY) {
		rememberedObjectSlotsDo(env, function, userData, walkFlags, parallel);
	}
#endif /* OMR_GC_MODRON_SCAVENGER */
}

/**
 * Walk all objects in the heap in a single threaded linear fashion.
 */
void
MM_HeapWalker::allObjectsDo(MM_EnvironmentBase *env, MM_HeapWalkerObjectFunc function, void *userData, uintptr_t walkFlags, bool parallel, bool prepareHeapForWalk)
{
	uintptr_t typeFlags = 0;

	GC_OMRVMInterface::flushCachesForWalk(env->getOmrVM());

	if (walkFlags & J9_MU_WALK_NEW_AND_REMEMBERED_ONLY) {
		typeFlags |= MEMORY_TYPE_NEW;
	}

	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_HeapRegionManager *regionManager = extensions->heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	MM_HeapRegionDescriptor *region = NULL;
	OMR_VMThread *omrVMThread = env->getOmrVMThread();
	
	while (NULL != (region = regionIterator.nextRegion())) {
		if (typeFlags == (region->getTypeFlags() & typeFlags)) {
			/* Optimization to avoid virtual dispatch for every slot in the system */
			omrobjectptr_t object = NULL;
			GC_ObjectHeapIteratorAddressOrderedList liveObjectIterator(extensions, region, false);

			while (NULL != (object = liveObjectIterator.nextObject())) {
				function(omrVMThread, region, object, userData);
			}
		}
	}
}
