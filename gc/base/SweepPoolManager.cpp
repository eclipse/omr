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
#include "modronopt.h"

#include "EnvironmentBase.hpp"
#include "SweepPoolManager.hpp"

/**
 * Free the receiver and all associated resources.
 */
void
MM_SweepPoolManager::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Tear down internal structures.
 */
void
MM_SweepPoolManager::tearDown(MM_EnvironmentBase *env)
{
	
}

bool
MM_SweepPoolManager::initialize(MM_EnvironmentBase *env)
{
	return true;
}

