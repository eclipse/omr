/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
