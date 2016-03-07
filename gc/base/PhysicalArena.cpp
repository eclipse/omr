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


#include "PhysicalArena.hpp"

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "Heap.hpp"

/**
 * Determine whether the child sub arena is allowed to expand according to the description given.
 * 
 * @return true if the expansion is allowed, false otherwise.
 */
bool
MM_PhysicalArena::canExpand(MM_EnvironmentBase *env, MM_PhysicalSubArena *expandArena)
{
	return true;
}

/**
 * Destroy and delete the instance.
 */
void
MM_PhysicalArena::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the receivers internal structures.
 * @return true on success, false otherwise.
 */
bool
MM_PhysicalArena::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Cleanup internal structures of the receiver.
 */
void
MM_PhysicalArena::tearDown(MM_EnvironmentBase *env)
{
	if(_attached) {
		_heap->detachArena(env, this);
	}
}
