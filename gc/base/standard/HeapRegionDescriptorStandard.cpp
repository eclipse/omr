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
#include "ModronAssertions.h"

#include "Configuration.hpp"
#include "EnvironmentStandard.hpp"
#include "HeapRegionDescriptorStandard.hpp"

MM_HeapRegionDescriptorStandard::MM_HeapRegionDescriptorStandard(MM_EnvironmentStandard *env, void *lowAddress, void *highAddress)
	: MM_HeapRegionDescriptor(env, lowAddress, highAddress)
{
	_typeId = __FUNCTION__;
}

bool 
MM_HeapRegionDescriptorStandard::initialize(MM_EnvironmentBase *envBase, MM_HeapRegionManager *regionManager)
{
	MM_EnvironmentStandard *env =  MM_EnvironmentStandard::getEnvironment(envBase);

	if (!MM_HeapRegionDescriptor::initialize(env, regionManager)) {
		return false;
	}

	return env->getExtensions()->configuration->initializeHeapRegionDescriptor(env, this);
}

void 
MM_HeapRegionDescriptorStandard::tearDown(MM_EnvironmentBase *env)
{
	env->getExtensions()->configuration->teardownHeapRegionDescriptor(env, this);

	MM_HeapRegionDescriptor::tearDown(env);
}

bool 
MM_HeapRegionDescriptorStandard::initializer(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor, void *lowAddress, void *highAddress)
{
	new((MM_HeapRegionDescriptorStandard*)descriptor) MM_HeapRegionDescriptorStandard((MM_EnvironmentStandard*)env, lowAddress, highAddress);
	return ((MM_HeapRegionDescriptorStandard*)descriptor)->initialize(env, regionManager);
}

void 
MM_HeapRegionDescriptorStandard::destructor(MM_EnvironmentBase *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor)
{
	((MM_HeapRegionDescriptorStandard*)descriptor)->tearDown((MM_EnvironmentStandard*)env);
}

