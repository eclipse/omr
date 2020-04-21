/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

#include "omr.h"
#include "omrgc.h"
#include "objectdescription.h"

#include "AllocateInitialization.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "omrgcstartup.hpp"
#include "ModronAssertions.h"

omrobjectptr_t
OMR_GC_AllocateObject(OMR_VMThread * omrVMThread, MM_AllocateInitialization *allocator)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
        Assert_MM_true(NULL != env->getExtensions()->getGlobalCollector());
	return allocator->allocateAndInitializeObject(omrVMThread);
}

omrobjectptr_t
OMR_GC_AllocateObject(OMR_VMThread * omrVMThread, uintptr_t allocationCategory, uintptr_t requiredSizeInBytes, uintptr_t allocationFlags)
{
	MM_AllocateInitialization allocator(MM_EnvironmentBase::getEnvironment(omrVMThread), allocationCategory, requiredSizeInBytes, allocationFlags);
	return OMR_GC_AllocateObject(omrVMThread, &allocator);
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
