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
