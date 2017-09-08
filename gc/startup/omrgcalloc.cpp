/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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
