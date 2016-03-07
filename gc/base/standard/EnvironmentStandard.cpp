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

#define J9_EXTERNAL_TO_VM

#include "omrcfg.h"

#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"

MM_EnvironmentStandard *
MM_EnvironmentStandard::newInstance(MM_GCExtensionsBase *extensions, OMR_VMThread *omrVMThread)
{
	void *envPtr;
	MM_EnvironmentStandard *env = NULL;
	
	envPtr = (void *)pool_newElement(extensions->environments);
	if (envPtr) {
		env = new(envPtr) MM_EnvironmentStandard(omrVMThread);
		if (!env->initialize(extensions)) {
			env->kill();
			env = NULL;	
		}
	}	

	return env;
}

bool
MM_EnvironmentStandard::initialize(MM_GCExtensionsBase *extensions)
{
	/* initialize base class */
	return MM_EnvironmentBase::initialize(extensions);
}

void
MM_EnvironmentStandard::tearDown(MM_GCExtensionsBase *extensions)
{
	/* tearDown base class */
	MM_EnvironmentBase::tearDown(extensions);
}
