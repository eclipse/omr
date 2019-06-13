/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#include "Forge.hpp"

#include "omrcomp.h"
#include "EnvironmentBase.hpp"

namespace OMR {
namespace GC {

bool
Forge::initialize(OMRPortLibrary* port)
{
	_portLibrary = port;
	return true;
}

void 
Forge::tearDown()
{
	_portLibrary = NULL;
}

/**
 * Allocates the amount of memory requested in bytesRequested.  Returns a pointer to the allocated memory, or NULL if the request could
 * not be performed.  This function is a wrapper of omrmem_allocate_memory.
 *
 * @param[in] byesRequested - the number of bytes to allocate
 * @param[in] category - the memory usage category for the allocated memory
 * @param[in] callsite - the origin of the memory request (e.g. filename.c:5), which should be found using the OMR_GET_CALLSITE() macro
 * @return a pointer to the allocated memory, or NULL if the request could not be performed
 */
void* 
Forge::allocate(std::size_t bytesRequested, OMR::GC::AllocationCategory::Enum category, const char* callsite)
{
	return _portLibrary->mem_allocate_memory(_portLibrary, bytesRequested, callsite, OMRMEM_CATEGORY_MM);
}

/**
 * Deallocate memory that has been allocated by the garbage collector.  This function should not be called to deallocate memory that has
 * not been allocated by either the allocate or reallocate functions.  This function is a wrapper of omrmem_free_memory.
 *
 * @param[in] memoryPointer - a pointer to the memory that will be freed
 */
void 
Forge::free(void* memoryPointer)
{	
	if (NULL == memoryPointer) {
		return;
	}
	
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	omrmem_free_memory(memoryPointer);
}

} // namespace GC
} // namespace OMR
