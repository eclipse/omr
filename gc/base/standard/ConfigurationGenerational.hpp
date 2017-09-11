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

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(CONFIGURATIONGENERATIONAL_HPP_)
#define CONFIGURATIONGENERATIONAL_HPP_

#include "omrcfg.h"

#include "ConfigurationStandard.hpp"

#if defined(OMR_GC_MODRON_SCAVENGER)

class MM_EnvironmentBase;
class MM_MemorySubSpaceSemiSpace;
class MM_Scavenger;

class MM_ConfigurationGenerational : public MM_ConfigurationStandard
{
/* Data members / Types */
public:
protected:
private:

/* Methods */
public:
	static MM_Configuration *newInstance(MM_EnvironmentBase *env);

	virtual MM_MemorySpace *createDefaultMemorySpace(MM_EnvironmentBase *env, MM_Heap *heap, MM_InitializationParameters *parameters);
	virtual MM_Heap *createHeapWithManager(MM_EnvironmentBase *env, UDATA heapBytesRequested, MM_HeapRegionManager *regionManager);

	virtual void defaultMemorySpaceAllocated(MM_GCExtensionsBase *extensions, void* defaultMemorySpace);
	
	MM_ConfigurationGenerational(MM_EnvironmentBase *env)
		: MM_ConfigurationStandard(env, gc_policy_gencon, calculateDefaultRegionSize(env))
	{
		_typeId = __FUNCTION__;
	};
	
protected:
	MM_MemorySubSpaceSemiSpace *createSemiSpace(MM_EnvironmentBase *envBase, MM_Heap *heap, MM_Scavenger *scavenger, MM_InitializationParameters *parameters, UDATA numaNode = UDATA_MAX);
private:
	uintptr_t calculateDefaultRegionSize(MM_EnvironmentBase *env);
};

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

#endif /* CONFIGURATIONGENERATIONAL_HPP_ */
