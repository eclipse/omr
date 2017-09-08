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



/**
 * @file
 * @ingroup GC_Modron_Base
 */

#if !defined(PHYSICALARENREGIONBASED_HPP_)
#define PHYSICALARENREGIONBASED_HPP_


#include "PhysicalArena.hpp"

class MM_EnvironmentBase;
class MM_MemorySpace;
class MM_PhysicalSubArena;
class MM_PhysicalSubArenaRegionBased;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Base
 */
class MM_PhysicalArenaRegionBased : public MM_PhysicalArena
{
public:
protected:
	MM_PhysicalSubArenaRegionBased *_physicalSubArena; /**< The subarena attached to this arena (currently only one but more will be required before phase4 is over to support gencon) */
private:
	
public:
	static MM_PhysicalArenaRegionBased *newInstance(MM_EnvironmentBase *env, MM_Heap *heap);

	virtual bool inflate(MM_EnvironmentBase *env);
	
	virtual bool attachSubArena(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, uintptr_t size, uintptr_t attachPolicy);
	virtual void detachSubArena(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena);

	bool canResize(MM_EnvironmentBase *env, MM_PhysicalSubArenaRegionBased *subArena, uintptr_t sizeDelta);
	uintptr_t maxExpansion(MM_EnvironmentBase *env,  MM_PhysicalSubArenaRegionBased *subArena);

	MM_PhysicalArenaRegionBased(MM_EnvironmentBase *env, MM_Heap *heap) :
		MM_PhysicalArena(env, heap),
		_physicalSubArena(NULL)
	{
		_typeId = __FUNCTION__;
	};
protected:
	bool initialize(MM_EnvironmentBase *env);
private:
};

#endif /* PHYSICALARENREGIONBASED_HPP_ */
