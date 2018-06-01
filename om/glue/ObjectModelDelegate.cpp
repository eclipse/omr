/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "OMR/Om/Allocation.hpp"
#include "ObjectModel.hpp"
#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Context.inl.hpp>
#include <OMR/Om/RootRef.hpp>

namespace OMR {
namespace Om {
#if 0
omrobjectptr_t
GC_ObjectModelDelegate::initializeAllocation(MM_EnvironmentBase *env, void *allocatedBytes, MM_AllocateInitialization *allocateInitialization)
{
	Assert_MM_true(MM_ObjectAllocationModel::allocation_category_example == allocateInitialization->getAllocationCategory());

	omrobjectptr_t objectPtr = NULL;
	if (NULL != allocatedBytes) {
		MM_ObjectAllocationModel *objectAllocationModel = (MM_ObjectAllocationModel *)allocateInitialization;
		objectPtr = objectAllocationModel->initializeObject(env, allocatedBytes);
	}
	return objectPtr;
}
#endif

omrobjectptr_t ObjectModelDelegate::initializeAllocation(MM_EnvironmentBase* env,
                                                         void* allocatedBytes,
                                                         MM_AllocateInitialization* a) {
	auto& cx = getContext(env);
	auto cell = (Cell*)allocatedBytes;
	auto alloc = (Allocation*)a;
	return (omrobjectptr_t)alloc->initializeObject(cx, cell);
}

#if defined(OMR_GC_MODRON_SCAVENGER)
void ObjectModelDelegate::calculateObjectDetailsForCopy(MM_EnvironmentBase* env,
                                                        MM_ForwardedHeader* forwardedHeader,
                                                        uintptr_t* objectCopySizeInBytes,
                                                        uintptr_t* reservedObjectSizeInBytes,
                                                        uintptr_t* hotFieldAlignmentDescriptor) {
	*objectCopySizeInBytes = getForwardedObjectSizeInBytes(forwardedHeader);
	*reservedObjectSizeInBytes =
	        env->getExtensions()->objectModel.adjustSizeInBytes(*objectCopySizeInBytes);
	*hotFieldAlignmentDescriptor = 0;
}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

} // namespace Om
} // namespace OMR
