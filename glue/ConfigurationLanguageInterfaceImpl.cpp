/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#include "AllocationInterfaceGeneric.hpp"
#include "ConfigurationLanguageInterfaceImpl.hpp"
#include "EnvironmentLanguageInterfaceImpl.hpp"
#include "ParallelGlobalGC.hpp"
#include "TLHAllocationInterface.hpp"

/**
 * Initialization
 */
MM_ConfigurationLanguageInterfaceImpl *
MM_ConfigurationLanguageInterfaceImpl::newInstance(MM_EnvironmentBase *env)
{
	MM_ConfigurationLanguageInterfaceImpl *configurationLanguageInterface = NULL;

	configurationLanguageInterface = (MM_ConfigurationLanguageInterfaceImpl *)env->getForge()->allocate(sizeof(MM_ConfigurationLanguageInterfaceImpl), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != configurationLanguageInterface) {
		new(configurationLanguageInterface) MM_ConfigurationLanguageInterfaceImpl();
		if (!configurationLanguageInterface->initialize(env)) {
			configurationLanguageInterface->kill(env);
			configurationLanguageInterface = NULL;
		}
	}

	return configurationLanguageInterface;
}

void
MM_ConfigurationLanguageInterfaceImpl::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the configuration's internal structures and values.
 * @return true if initialization completed, false otherwise
 */
bool
MM_ConfigurationLanguageInterfaceImpl::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Free any internal structures associated to the receiver.
 */
void
MM_ConfigurationLanguageInterfaceImpl::tearDown(MM_EnvironmentBase *env)
{
}

/**
 * Initialize the Environment with the appropriate values.
 * @Note if this function is overridden the overridding function
 * has to call super::initializeEnvironment before it does any other work.
 *
 * @param env The environment to initialize
 */
bool
MM_ConfigurationLanguageInterfaceImpl::initializeEnvironment(MM_EnvironmentBase* env)
{
	return true;
}

/**
 * Initialize and return the appropriate object allocation interface for the given Environment.
 * Given the allocation scheme and particular environment, select and return the appropriate object allocation interface.
 * @return new subtype instance of MM_ObjectAllocationInterface, or NULL on failure.
 */
MM_ObjectAllocationInterface*
MM_ConfigurationLanguageInterfaceImpl::createObjectAllocationInterface(MM_EnvironmentBase* env)
{
	return MM_TLHAllocationInterface::newInstance(env);
}

MM_EnvironmentLanguageInterface *
MM_ConfigurationLanguageInterfaceImpl::createEnvironmentLanguageInterface(MM_EnvironmentBase *env)
{
	return MM_EnvironmentLanguageInterfaceImpl::newInstance(env);
}

bool
MM_ConfigurationLanguageInterfaceImpl::initializeArrayletLeafSize(MM_EnvironmentBase* env)
{
	return true;
}

void
MM_ConfigurationLanguageInterfaceImpl::initializeWriteBarrierType(MM_EnvironmentBase* env, uintptr_t configWriteBarrierType)
{
}

void
MM_ConfigurationLanguageInterfaceImpl::initializeAllocationType(MM_EnvironmentBase* env, uintptr_t configGcAllocationType)
{
}

MM_GlobalCollector *
MM_ConfigurationLanguageInterfaceImpl::createGlobalCollector(MM_EnvironmentBase *env)
{
	return MM_ParallelGlobalGC::newInstance(env, MM_GCExtensionsBase::getExtensions(env->getOmrVM())->collectorLanguageInterface);
}

void
MM_ConfigurationLanguageInterfaceImpl::initializeSaltData(MM_EnvironmentBase *env, uintptr_t index, uint32_t startValue)
{
}

uintptr_t
MM_ConfigurationLanguageInterfaceImpl::internalGetWriteBarrierType(MM_EnvironmentBase* env)
{
	return 0;
}

uintptr_t
MM_ConfigurationLanguageInterfaceImpl::internalGetAllocationType(MM_EnvironmentBase* env)
{
	return 0;
}
