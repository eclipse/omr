/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2014
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

#include "AllocateDescription.hpp"
#include "Debug.hpp"
#include "EnvironmentLanguageInterface.hpp"
#include "MemorySubSpace.hpp"
#include "ModronAssertions.h"

void
MM_AllocateDescription::saveObjects(MM_EnvironmentBase* env)
{
	if (NULL != _spine) {
		bool result = env->_envLanguageInterface->saveObjects((omrobjectptr_t)_spine);

		/* The caller should have handled this */
		Assert_MM_true(result);
	}
}

void
MM_AllocateDescription::restoreObjects(MM_EnvironmentBase* env)
{
	if (NULL != _spine) {
		env->_envLanguageInterface->restoreObjects((omrobjectptr_t*)&_spine);
	}
}
