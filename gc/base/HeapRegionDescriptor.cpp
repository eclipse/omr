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

#include "omrcfg.h"
#include "omrcomp.h"

#include "HeapRegionDescriptor.hpp"

#include "MemoryPool.hpp"
#include "MemorySubSpace.hpp"

MM_HeapRegionDescriptor::MM_HeapRegionDescriptor(MM_EnvironmentBase *env, void *lowAddress, void *highAddress)
	: _regionsInSpan(0)
	, _heapRegionDescriptorExtension(NULL)
	, _lowAddress(lowAddress)
	, _highAddress(highAddress)
	, _previousRegion(NULL)
	, _nextRegion(NULL)
	, _previousRegionInSubSpace(NULL)
	, _nextRegionInSubSpace(NULL)
	, _nextInSet(NULL)
	, _isAllocated(false)
	, _memorySubSpace(NULL)
	, _regionType(MM_HeapRegionDescriptor::RESERVED)
	, _memoryPool(NULL)
	, _numaNode(0)
	, _regionProperties(MM_HeapRegionDescriptor::MANAGED)
{
	_typeId = __FUNCTION__;
	_headOfSpan = this;
}

void
MM_HeapRegionDescriptor::associateWithSubSpace(MM_MemorySubSpace *subSpace)
{
	Assert_MM_true(NULL != subSpace);
	Assert_MM_true(NULL == _memorySubSpace);
	_memorySubSpace = subSpace;
	_memorySubSpace->registerRegion(this);
}

void
MM_HeapRegionDescriptor::disassociateWithSubSpace()
{
	if (NULL != _memorySubSpace) {
		_memorySubSpace->unregisterRegion(this);
		_memorySubSpace = NULL;
	}
}

bool 
MM_HeapRegionDescriptor::initialize(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager)
{
	return true;
}

void 
MM_HeapRegionDescriptor::tearDown(MM_EnvironmentBase *env)
{
	
}

void
MM_HeapRegionDescriptor::reinitialize(MM_EnvironmentBase *env, void *lowAddress, void *highAddress)
{
	_lowAddress = lowAddress;
	_highAddress = highAddress;
}

bool 
MM_HeapRegionDescriptor::initializer(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor, void *lowAddress, void *highAddress)
{
	new(descriptor) MM_HeapRegionDescriptor(env, lowAddress, highAddress);
	return descriptor->initialize(env, regionManager);
}

void 
MM_HeapRegionDescriptor::destructor(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor)
{
	descriptor->tearDown(env);
}
