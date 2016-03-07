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


#include "PhysicalSubArenaVirtualMemory.hpp"

class MM_EnvironmentBase;

/**
 * Zos390 Platform dependent trickery for the ar command.  Without this definition
 * the ar command on Zos390 will fail to link this class.
 */
#if defined(J9ZOS390)
#include "ZOSLinkage.hpp"
int j9zos390LinkTrickPhysicalSubArenaVirtualMemory;
#endif /* J9ZOS390 */

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
