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
 *******************************************************************************/

#include "omrcfg.h"
#include "ModronAssertions.h"

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

	MM_CollectorLanguageInterface *cli = MM_GCExtensionsBase::getExtensions(env->getOmrVM())->collectorLanguageInterface;
	return cli->collectorHeapRegionDescriptorInitialize(env, this);
}

void 
MM_HeapRegionDescriptorStandard::tearDown(MM_EnvironmentBase *env)
{
	MM_CollectorLanguageInterface *cli = MM_GCExtensionsBase::getExtensions(env->getOmrVM())->collectorLanguageInterface;
	cli->collectorHeapRegionDescriptorTearDown(env, this);

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

