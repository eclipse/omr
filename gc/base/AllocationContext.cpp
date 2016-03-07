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
#include "modronopt.h"

#include "AllocationContext.hpp"

#include "EnvironmentBase.hpp"
#include "ModronAssertions.h"

/**
 * Initialization.
 */
bool
MM_AllocationContext::initialize(MM_EnvironmentBase *env)
{
	return true;
}

void
MM_AllocationContext::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialization.
 */
void
MM_AllocationContext::tearDown(MM_EnvironmentBase *env)
{
}
