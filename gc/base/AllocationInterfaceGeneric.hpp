/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#if !defined(ALLOCATIONINTERFACEGENERiC_HPP_)
#define ALLOCATIONINTERFACEGENERiC_HPP_

#include "omr.h"

#include "ObjectAllocationInterface.hpp"

class MM_AllocationInterfaceGeneric : public MM_ObjectAllocationInterface
{
public:
protected:
private:
	bool _cachedAllocationsEnabled; /**< Are cached allocations enabled? */

public:
	static MM_AllocationInterfaceGeneric* newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
	virtual void *allocateArray(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
	virtual void *allocateArrayletSpine(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);

	virtual void flushCache(MM_EnvironmentBase *env);

	virtual void enableCachedAllocations(MM_EnvironmentBase* env);
	virtual void disableCachedAllocations(MM_EnvironmentBase* env);
	virtual bool cachedAllocationsEnabled(MM_EnvironmentBase* env) { return _cachedAllocationsEnabled; }

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	MM_AllocationInterfaceGeneric(MM_EnvironmentBase *env) :
		MM_ObjectAllocationInterface(env),
		_cachedAllocationsEnabled(false)
	{
		_typeId = __FUNCTION__;
	};

private:

};

#endif /*ALLOCATIONINTERFACEGENERiC_HPP_*/
