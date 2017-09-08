/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#if !defined(PHYSICALSUBARENAREGIONBASED_HPP_)
#define PHYSICALSUBARENAREGIONBASED_HPP_

#include "GCExtensionsBase.hpp"

#include "PhysicalSubArena.hpp"

class MM_MemorySubSpace;
class MM_PhysicalSubArenaRegionBased;
class MM_PhysicalArena;

struct J9MemoryNodeDetail;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Base
 */
class MM_PhysicalSubArenaRegionBased: public MM_PhysicalSubArena
{
	/*
	 * Data members
	 */
public:
protected:
	MM_PhysicalSubArenaRegionBased *_nextSubArena; /**< next subarena in the list of attached subarenas */

private:
#if defined (OMR_GC_VLHGC)
	J9MemoryNodeDetail const *_affinityLeaders;	/**< A subset of the nodes in _activeNodes which represent the nodes which are offering both physical memory and CPU */
#endif /* defined (OMR_GC_VLHGC) */
	uintptr_t _affinityLeaderCount;	/**< The number of elements in the _affinityLeaders array */
	uintptr_t _nextNUMAIndex;	/**< The index into the _affinityLeaders array from which we must expand the next region */
	MM_GCExtensionsBase * _extensions;	/** <a cached pointer to the extensions structure */

	/*
	 * Function members
	 */
public:
	static MM_PhysicalSubArenaRegionBased *newInstance(MM_EnvironmentBase *env, MM_Heap *heap);
	virtual void kill(MM_EnvironmentBase *env);
	virtual bool initialize(MM_EnvironmentBase *env);
	
	virtual bool inflate(MM_EnvironmentBase *env);
	virtual uintptr_t expand(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual uintptr_t performExpand(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual uintptr_t contract(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual bool canContract(MM_EnvironmentBase *env);
	virtual uintptr_t getAvailableContractionSize(MM_EnvironmentBase *env, MM_MemorySubSpace *memorySubSpace, MM_AllocateDescription *allocDescription);
	
	MMINLINE MM_PhysicalSubArenaRegionBased *getNextSubArena() { return _nextSubArena; }
	MMINLINE void setNextSubArena(MM_PhysicalSubArenaRegionBased *nextSubArena) { _nextSubArena = nextSubArena; }

	MM_PhysicalSubArenaRegionBased(MM_Heap *heap);
	
protected:
	uintptr_t doExpandInSubSpace(MM_EnvironmentBase *env, uintptr_t expandSize, MM_MemorySubSpace *subspace);
	uintptr_t doContractInSubSpace(MM_EnvironmentBase *env, uintptr_t contractSize, MM_MemorySubSpace *subspace);
	virtual void tearDown(MM_EnvironmentBase *env);
private:
	/**
	 * If NUMA is enabled, get the next NUMA node number to be used for expansion.
	 * This has the side-effect of updating _nextNUMAIndex
	 * 
	 * @return The next NUMA node to use, or 0 if NUMA is disabled
	 */
	uintptr_t getNextNumaNode();
	
	/**
	 * If NUMA is enabled, get the previous NUMA node number (the next node to use for contraction).
	 * This has the side-effect of updating _nextNUMAIndex
	 * 
	 * @return The previous NUMA node used, or 0 if NUMA is disabled
	 */
	uintptr_t getPreviousNumaNode();
	
	/**
	 * Called after expand or contract to validate the the NUMA characteristics of the heap are
	 * symmetrical. That is, memory is evenly balanced between nodes.
	 * @param env[in] the current thread
	 */
	void validateNumaSymmetry(MM_EnvironmentBase *env);

};

#endif /* PHYSICALSUBARENAREGIONBASED_HPP_ */
