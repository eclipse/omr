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

#include "omr.h"
#include "omrgc.h"
#include "objectdescription.h"

#include "AllocateInitialization.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "omrgcstartup.hpp"

omrobjectptr_t
OMR_GC_AllocateObject(OMR_VMThread * omrVMThread, MM_AllocateInitialization *allocator)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

	/* TODO: Deprecate this -- required for early versions of ruby/omr integration */
	if (allocator->isGCAllowed() && (NULL == env->getExtensions()->getGlobalCollector())) {
		/* Lazily create the collector before attempting to allocate. */
		if (OMR_ERROR_NONE != OMR_GC_InitializeCollector(omrVMThread)) {
			return NULL;
		}
	}

	return allocator->allocateAndInitializeObject(omrVMThread);
}

omr_error_t
OMR_GC_SystemCollect(OMR_VMThread* omrVMThread, uint32_t gcCode)
{
	omr_error_t result = OMR_ERROR_NONE;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	MM_GCExtensionsBase *extensions = env->getExtensions();
	if (NULL == extensions->getGlobalCollector()) {
		result = OMR_GC_InitializeCollector(omrVMThread);
	}
	if (OMR_ERROR_NONE == result) {
		extensions->heap->systemGarbageCollect(env, gcCode);
	}
	return result;
}
