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


#include "PhysicalSubArenaVirtualMemory.hpp"

class MM_EnvironmentBase;

bool
MM_PhysicalSubArenaVirtualMemory::initialize(MM_EnvironmentBase* env)
{
	if (!MM_PhysicalSubArena::initialize(env)) {
		return false;
	}

	return true;
}

/**
 * Find the next valid address higher than the current physical subarenas memory.
 * This routine is typically used for decommit purposes, to find the valid ranges surrounding a particular
 * address range.
 * @return the next highest valid range, or NULL if there is none.
 */
void*
MM_PhysicalSubArenaVirtualMemory::findAdjacentHighValidAddress(MM_EnvironmentBase* env)
{
	/* Is there a valid higher address? */
	if (NULL == _highArena) {
		return NULL;
	}

	/* There is - return its lowest address */
	return _highArena->getLowAddress();
}
