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

#if !defined(FORGE_HPP_)
#define FORGE_HPP_

#include "omrcfg.h"

#include "omrcomp.h"
#include "thread_api.h"
#include "omrport.h"
#include "AllocationCategory.hpp"

#include <new>

class MM_EnvironmentBase;
class MM_GCExtensionsBase;

namespace OMR {
namespace GC {

class Forge {

/* Data Members */
private:
	OMRPortLibrary* _portLibrary;
	
/* Function Members */
public:
	/**
	 * Initialize internal structures of the memory forge.  An instance of Forge must be initialized before
	 * the methods allocate or free are called.
	 * 
	 * @param[in] port The portlibrary the forge will make allocations through.
	 * @return true if the forge was successfully initialized, otherwise returns false.  A instance of Forge
	 * 		   should not be used before it has been successfully initialized.
	 */
	bool initialize(OMRPortLibrary* port);
	
	/**
	 * Release any internal structures of the memory forge.  After tear down, there should be no calls to the 
	 * methods allocate or free.
	 * 
	 */
	void tearDown();

	/**
	 * Allocates the amount of memory requested in bytesRequested.  Returns a pointer to the allocated memory, 
	 * or NULL if the request could not be performed.  This function is a wrapper of omrmem_allocate_memory.
	 *
	 * @param[in] byesRequested - the number of bytes to allocate
	 * @param[in] category - the memory usage category for the allocated memory
	 * @param[in] callsite - the origin of the memory request (e.g. filename.c:5), which should be found using
	 * 						 the OMR_GET_CALLSITE() macro
	 * @return a pointer to the allocated memory, or NULL if the request could not be performed
	 */
	void* allocate(std::size_t bytesRequested, AllocationCategory::Enum category, const char* callsite);

	/**
	 * Deallocate memory that has been allocated by the garbage collector.  This function should not be called
	 * to deallocate memory that has not been allocated by either the allocate or reallocate functions.  This 
	 * function is a wrapper of omrmem_free_memory.
	 *
	 * @param[in] memoryPointer - a pointer to the memory that will be freed
	 */
	void free(void* memoryPointer);
};

} // namespace GC
} // namespace OMR

/**
 * Usage:
 *   MyStruct* s = new(forge, AllocationCategory::FIXED, OMR_GET_CALLSITE(), std::nothrow) MyStruct(constructor params...);
 */
inline void*
operator new(std::size_t size, OMR::GC::Forge* forge, OMR::GC::AllocationCategory::Enum category, const char* site, const std::nothrow_t&) throw() {
	return forge->allocate(size, category, site);
}

/**
 * Usage:
 *   MyStruct* s = new(forge, AllocationCategory::FIXED, OMR_GET_CALLSITE(), std::nothrow) MyStruct(constructor params...);
 */
inline void*
operator new[](std::size_t size, OMR::GC::Forge* forge, OMR::GC::AllocationCategory::Enum category, const char* site, const std::nothrow_t&) throw() {
	return operator new(size, forge, category, site, std::nothrow);
}

/**
 * Backwards compatible alias.
 */
typedef OMR::GC::Forge MM_Forge;

#endif /*FORGE_HPP_*/
