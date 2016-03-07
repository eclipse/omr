/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 ******************************************************************************/

#include "omrcfg.h"
#include "omrcomp.h"

#include "HeapRegionDescriptor.hpp"

#include "MemoryPool.hpp"
#include "MemorySubSpace.hpp"

MM_HeapRegionDescriptor::MM_HeapRegionDescriptor(MM_EnvironmentBase *env, void *lowAddress, void *highAddress)
	: _regionsInSpan(0)
	, _lowAddress(lowAddress)
	, _highAddress(highAddress)
	, _previousRegion(NULL)
	, _nextRegion(NULL)
	, _memorySubSpace(NULL)
	, _regionType(MM_HeapRegionDescriptor::RESERVED)
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
