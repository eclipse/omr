/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/** 
 * @file
 * @ingroup GC_Structs
 */

#if !defined(SUBLISTPOOL_HPP_)
#define SUBLISTPOOL_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrthread.h"
#include "modronbase.h"

#include "AtomicOperations.hpp"
#include "Forge.hpp"

class MM_EnvironmentBase;
class MM_SublistFragment;
class MM_SublistPuddle;

class GC_SublistIterator;

/**
 * A thread-safe growable list that supports batch-reservation of uintptr_t-sized elements.
 * An MM_SublistPool is a pool of memory which consists of a linked list of zero or 
 * more <i>puddles</i> (instances of MM_SublistPuddle). A thread can reserve a block
 * of memory from the list (an instance of MM_SublistFragment), and then operate without
 * contention on that fragment.
 */
class MM_SublistPool
{
/*
 * Data members
 */
private:
	MM_SublistPuddle *_list;
	MM_SublistPuddle *_allocPuddle;
	omrthread_monitor_t _mutex;
	uintptr_t _growSize;
	uintptr_t _currentSize;
	uintptr_t _maxSize;
	volatile uintptr_t _count; /**< A count for number of elements across all sublistPuddles */
	OMR::GC::AllocationCategory::Enum _allocCategory;
	
	MM_SublistPuddle *_previousList; /**< A list of the non-empty puddles when #startProcessingSublist() was called */
	
protected:
public:

/*
 * Function members
 */
private:
	MM_SublistPuddle *createNewPuddle(MM_EnvironmentBase *env);
	void freePuddles(MM_EnvironmentBase *env, MM_SublistPuddle *list);

protected:
public:
	bool initialize(MM_EnvironmentBase *env, OMR::GC::AllocationCategory::Enum category);
	void tearDown(MM_EnvironmentBase *env);

	MMINLINE void setGrowSize(uintptr_t growSize) { _growSize = growSize; }
	MMINLINE uintptr_t getGrowSize() { return _growSize; }
	MMINLINE void setMaxSize(uintptr_t maxSize) { _maxSize = maxSize; }
	MMINLINE uintptr_t getMaxSize() { return _maxSize; }
	
	MMINLINE void incrementCount(uintptr_t count)
	{
		if( 0 != count ) {
			MM_AtomicOperations::add(&_count, count);
		}		
	}

	MMINLINE void decrementCount(uintptr_t count)
	{
		if( 0 != count ) {
			MM_AtomicOperations::subtract(&_count, count);
		}		
	}
	
	uintptr_t countElements();

	MMINLINE bool isEmpty() { return _currentSize == 0 ? true : false; };

	bool allocate(MM_EnvironmentBase *env, MM_SublistFragment *fragment);
	uintptr_t *allocateElementNoContention(MM_EnvironmentBase *env);

	void compact(MM_EnvironmentBase *env);
	void clear(MM_EnvironmentBase *env);
	
	/**
	 * Prepare to process this sublist by moving all of its non-empty puddles onto
	 * the list of previous puddles. The puddles may be retrieved by calling #popPreviousPuddle().
	 */
	void startProcessingSublist();

	/**
	 * Pop a puddle from the list of puddles which were active when #startProcessingSublist() was called.
	 * Return returnedPuddle to the list of puddles. It should be a puddle returned by a previous call to this function. 
	 * This is protected by a lock, so may safely be called by multiple threads.
	 * 
	 * @param emptyPuddle[in] a puddle which has already been processed, or NULL
	 * @return a puddle to process, or NULL if the list is empty
	 */
	MM_SublistPuddle *popPreviousPuddle(MM_SublistPuddle * returnedPuddle);
	
	MM_SublistPool() 
		: _list(NULL)
		, _allocPuddle(NULL)
		, _mutex(NULL)
		, _growSize(0)
		, _currentSize(0)
		, _maxSize(0)
		, _count(0)
		, _allocCategory(OMR::GC::AllocationCategory::OTHER)
		, _previousList(NULL)
	{}

	friend class GC_SublistIterator;
};

#endif /* SUBLISTPOOL_HPP_ */
