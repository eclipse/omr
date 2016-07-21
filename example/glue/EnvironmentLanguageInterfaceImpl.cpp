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

#include "Collector.hpp"
#include "EnvironmentLanguageInterfaceImpl.hpp"
#include "EnvironmentStandard.hpp"
#include "Forge.hpp"
#include "GCExtensionsBase.hpp"


MM_EnvironmentLanguageInterfaceImpl::MM_EnvironmentLanguageInterfaceImpl(MM_EnvironmentBase *env)
	: MM_EnvironmentLanguageInterface(env)
{
	_typeId = __FUNCTION__;
};

MM_EnvironmentLanguageInterfaceImpl *
MM_EnvironmentLanguageInterfaceImpl::newInstance(MM_EnvironmentBase *env)
{
	MM_EnvironmentLanguageInterfaceImpl *eli = NULL;

	eli = (MM_EnvironmentLanguageInterfaceImpl *)env->getForge()->allocate(sizeof(MM_EnvironmentLanguageInterfaceImpl), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != eli ) {
		new(eli) MM_EnvironmentLanguageInterfaceImpl(env);
		if (!eli ->initialize(env)) {
			eli->kill(env);
			eli = NULL;
		}
	}

	return eli;
}

void
MM_EnvironmentLanguageInterfaceImpl::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the collector's internal structures and values.
 * @return true if initialization completed, false otherwise
 */
bool
MM_EnvironmentLanguageInterfaceImpl::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Free any internal structures associated to the receiver.
 */
void
MM_EnvironmentLanguageInterfaceImpl::tearDown(MM_EnvironmentBase *env)
{
}

bool
MM_EnvironmentLanguageInterfaceImpl::saveObjects(omrobjectptr_t objectPtr)
{
	Assert_MM_true(NULL == _omrThread->savedObject);
	_omrThread->savedObject = (void *)objectPtr;
	return true;
}

void
MM_EnvironmentLanguageInterfaceImpl::restoreObjects(omrobjectptr_t *objectPtrIndirect)
{
	Assert_MM_true(NULL != _omrThread->savedObject);
	*objectPtrIndirect = (omrobjectptr_t)_omrThread->savedObject;
	_omrThread->savedObject = NULL;
}

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
/**
 * Disable inline TLH allocates by hiding the real heap allocation address from
 * JIT/Interpreter in realHeapAlloc and setting heapALloc == HeapTop so TLH
 * looks full.
 *
 */
void
MM_EnvironmentLanguageInterfaceImpl::disableInlineTLHAllocate()
{
}

/**
 * Re-enable inline TLH allocate by restoring heapAlloc from realHeapAlloc
 */
void
MM_EnvironmentLanguageInterfaceImpl::enableInlineTLHAllocate()
{
}

/**
 * Determine if inline TLH allocate is enabled; its enabled if realheapAlloc is NULL.
 * @return TRUE if inline TLH allocates currently enabled for this thread; FALSE otheriwse
 */
bool
MM_EnvironmentLanguageInterfaceImpl::isInlineTLHAllocateEnabled()
{
	return false;
}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

void
MM_EnvironmentLanguageInterfaceImpl::parallelMarkTask_setup(MM_EnvironmentBase *env)
{
}

void
MM_EnvironmentLanguageInterfaceImpl::parallelMarkTask_cleanup(MM_EnvironmentBase *envBase)
{
}
