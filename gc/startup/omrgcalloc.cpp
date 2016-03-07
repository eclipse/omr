/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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

#include "omrmodroncore.h"
#include "omr.h"
#include "omrgc.h"
#include "objectdescription.h"

#include "AllocateDescription.hpp"
#include "EnvironmentBase.hpp"
#include "EnvironmentLanguageInterface.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "MemorySpace.hpp"
#include "ModronAssertions.h"
#include "ObjectAllocationInterface.hpp"
#include "ObjectModel.hpp"
#include "omrgcstartup.hpp"

omrobjectptr_t static inline
allocHelper(OMR_VMThread * omrVMThread, size_t sizeInBytes, uintptr_t flags, bool collectOnFailure)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

	uintptr_t vmState = env->pushVMstate(J9VMSTATE_GC_ALLOCATE_OBJECT);

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	if (!env->_envLanguageInterface->isInlineTLHAllocateEnabled()) {
		/* For duration of call restore TLH allocate fields;
		 * we will hide real heapAlloc again on exit to fool JIT/Interpreter
		 * into thinking TLH is full if needed
		 */
		env->_envLanguageInterface->enableInlineTLHAllocate();
	}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	uintptr_t allocatedFlags = 0;
	uintptr_t sizeAdjusted = extensions->objectModel.adjustSizeInBytes(sizeInBytes);
	bool threadAtSafePoint = J9_ARE_ALL_BITS_SET(flags, OMR_GC_THREAD_AT_SAFEPOINT);
	MM_AllocateDescription allocdescription(sizeAdjusted, allocatedFlags, collectOnFailure, threadAtSafePoint);

	/* OMRTODO: Under what conditions could this assert fail? */
	assert(omrVMThread->memorySpace == env->getMemorySpace());

	omrobjectptr_t heapBytes = (omrobjectptr_t)env->_objectAllocationInterface->allocateObject(env, &allocdescription, env->getMemorySpace(), collectOnFailure);

	/* OMRTODO: Should we use zero TLH instead of memset? */
	if (NULL != heapBytes) {
		if (J9_ARE_ALL_BITS_SET(flags, OMR_GC_ALLOCATE_ZERO_MEMORY)) {
			uintptr_t size = allocdescription.getBytesRequested();
			memset(heapBytes, 0, size);
		}
	}

	allocdescription.setAllocationSucceeded(NULL != heapBytes);
	/* Issue Allocation Failure Report if required */
	env->allocationFailureEndReportIfRequired(&allocdescription);

	if (collectOnFailure) {
		/* Done allocation - successful or not */
		/* OMRTODO: We are releasing exclusive before writing any of the object header, meaning
		 * it may not be walkable in the event of another GC or heap traversal. Is this
		 * the right place to release or should the caller be responsible? */
		env->unwindExclusiveVMAccessForGC();
	}

	env->popVMstate(vmState);

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	if (extensions->fvtest_disableInlineAllocation || extensions->instrumentableAllocateHookEnabled || extensions->disableInlineCacheForAllocationThreshold) {
		env->_envLanguageInterface->disableInlineTLHAllocate();
	}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	return heapBytes;
}

omrobjectptr_t
OMR_GC_Allocate(OMR_VMThread * omrVMThread, size_t sizeInBytes, uintptr_t flags)
{
	omrobjectptr_t heapBytes = allocHelper(omrVMThread, sizeInBytes, flags, true);
	if (NULL == heapBytes) {
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
		if (NULL == extensions->getGlobalCollector()) {
			/* Lazily create the collector and try to allocate again. */
			if (OMR_ERROR_NONE == OMR_GC_InitializeCollector(omrVMThread)) {
				heapBytes = allocHelper(omrVMThread, sizeInBytes, flags, true);
			}
		}
	}
	return heapBytes;
}

omrobjectptr_t
OMR_GC_AllocateNoGC(OMR_VMThread * omrVMThread, size_t sizeInBytes, uintptr_t flags)
{
	return allocHelper(omrVMThread, sizeInBytes, flags, false);
}

omr_error_t
OMR_GC_SystemCollect(OMR_VMThread* omrVMThread, uint32_t gcCode)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	/* Lazily create the collector, if necessary. */
	if (NULL == extensions->getGlobalCollector()) {
		if (OMR_ERROR_NONE != OMR_GC_InitializeCollector(omrVMThread)) {
			return OMR_ERROR_INTERNAL;
		}
	}
	extensions->heap->systemGarbageCollect(env, gcCode);
	return OMR_ERROR_NONE;
}
