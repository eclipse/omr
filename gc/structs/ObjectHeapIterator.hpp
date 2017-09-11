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

#if !defined(OBJECTHEAPITERATOR_HPP_)
#define OBJECTHEAPITERATOR_HPP_

#include "omrcfg.h"
#include "modronbase.h"
#include "objectdescription.h"

#include "Base.hpp"

/**
 * Generic interface for iterating over all objects and non-objects in a memory segment.
 * Subclasses should provide at least two constructors: one which takes one or more 
 * parameters indicating the chunk of heap to be walked, and one that does not.
 * If the latter constructor is used, the iterator is <i>uninitialized</i>
 * until reset() is called. An uninitialized iterator <b>must</b> return NULL
 * when nextObject() or nextObjectNoAdvance() is called.
 * @note After an iterator is initialized, a caller should either call nextObject() 
 * or nextObjectNoAdvance(), i.e. not mix and match calls to both.
 *  
 * @ingroup GC_Structs
 */
class GC_ObjectHeapIterator
{
public:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

	/**
	 * Return the next object in the heap.
	 * 
	 * @return the next object
	 * @return NULL if there are no more objects
	 * 
	 * @note nextObject() must return NULL for an uninitialized iterator
	 */
	virtual omrobjectptr_t nextObject() = 0;
	
	/**
	 * Return the next object, without advancing over it until the next
	 * call to this method.
	 * Some implementations of nextObject() might skip over the object to be
	 * returned from the current call, in preparation for the next call to
	 * nextObject(). If the validity of the objects is in question, this
	 * method should be used instead, ensuring that each object is valid before
	 * advancing over it.
	 * 
	 * @return the next object
	 * @return NULL if there are no more objects
	 * 
	 * @note nextObjectNoAdvance() must return NULL for an uninitialized
	 * iterator
	 */
	virtual omrobjectptr_t nextObjectNoAdvance() = 0;

	/**
	 * Advance the iterator.
	 * This method should be used only in combination with nextObjectNoAdvance(),
	 * and <b>not</b> nextObject().
	 * 
	 * @param size the number of bytes by which to advance the iterator
	 */
	virtual void advance(uintptr_t size) = 0;
	
	/**
	 * Reset the iterator to walk a new chunk of heap.
	 * 
	 * @note Assumes that <code>base</code> points either to the head of a
	 * valid object, or to some other structure understood by the
	 * object heap iterator (e.g. a free list entry)
	 */
	virtual void reset(uintptr_t *base, uintptr_t *top) = 0;
};

#endif /* OBJECTHEAPITERATOR_HPP_ */

