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
