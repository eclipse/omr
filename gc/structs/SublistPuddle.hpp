/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 * @ingroup GC_Structs
 */

#if !defined(SUBLISTPUDDLE_HPP_)
#define SUBLISTPUDDLE_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"

#include "AtomicOperations.hpp"
#include "Forge.hpp"

class MM_EnvironmentBase;
class MM_SublistFragment;
class MM_SublistPool;

/* Forward declaration of Friends */
class GC_SublistIterator;
class GC_SublistSlotIterator;

/**
 * A portion of memory allocated for an MM_SublistPool.
 * @ingroup GC_Structs
 */
class MM_SublistPuddle
{
/*
 * Data members
 */
private:
	MM_SublistPool *_parent;
		
	MM_SublistPuddle *_next;
	uintptr_t *_listBase;
	uintptr_t * volatile _listCurrent;
	uintptr_t *_listTop;

	uintptr_t _size;

protected:
public:
	
/*
 * Function members
 */
private:
	bool initialize(MM_EnvironmentBase *env, uintptr_t size, MM_SublistPool *parent);

protected:
public:
	static MM_SublistPuddle *newInstance(MM_EnvironmentBase *env, uintptr_t size, MM_SublistPool *parent, MM_AllocationCategory::Enum category);
	static void kill(MM_EnvironmentBase *env, MM_SublistPuddle *puddle);
	void tearDown(MM_EnvironmentBase *env) {};

	bool allocate(MM_SublistFragment *fragment);
	uintptr_t *allocateElementNoContention();
	void reset();

	MMINLINE bool isFull() { return _listCurrent == _listTop; }
	MMINLINE bool isEmpty() { return _listCurrent == _listBase; }
	MMINLINE uintptr_t consumedSize() { return ((uintptr_t)_listCurrent) - ((uintptr_t)_listBase); }
	MMINLINE uintptr_t freeSize() { return ((uintptr_t)_listTop) - ((uintptr_t)_listCurrent); }
	MMINLINE uintptr_t totalSize() { return ((uintptr_t)_listTop) - ((uintptr_t)_listBase); }

	MMINLINE MM_SublistPool *getParent() {return _parent; }

	void merge(MM_SublistPuddle *sourcePuddle);

	MMINLINE MM_SublistPuddle *getNext() { return _next; }
	MMINLINE void setNext(MM_SublistPuddle *next) { _next = next; }

	MM_SublistPuddle() {}

	friend class GC_SublistIterator;
	friend class GC_SublistSlotIterator;
};

#endif /* SUBLISTPUDDLE_HPP_ */
