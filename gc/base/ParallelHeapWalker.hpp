/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#if !defined(PARALLEL_HEAP_WALKER_HPP_)
#define PARALLEL_HEAP_WALKER_HPP_

#include "omr.h"
#include "omrcfg.h"

#include "HeapWalker.hpp"

class MM_EnvironmentBase;
class MM_ParallelGlobalGC;
class MM_MarkMap;

class MM_ParallelHeapWalker : public MM_HeapWalker
{
	/*
	 * Data members
	 */
private:
	MM_MarkMap *_markMap;
	MM_ParallelGlobalGC *_globalCollector;
protected:
public:
	
	/*
	 * Function members
	 */
private:
protected:
public:	
	/**
	 * Walk through all live objects of the heap in parallel and apply the provided function.
	 */
	void allObjectsDoParallel(MM_EnvironmentBase *env, MM_HeapWalkerObjectFunc function, void *userData, UDATA walkFlags);

	/**
	 * Walk through all live objects of the heap and apply the provided function.
	 * If parallel is set to true, task is dispatched to GC threads and walks the heap segments in parallel,
	 * otherwise walk all objects in the heap in a single threaded linear fashion.
	 */
	virtual void allObjectsDo(MM_EnvironmentBase *env, MM_HeapWalkerObjectFunc function, void *userData, UDATA walkFlags, bool parallel, bool prepareHeapForWalk);

	MM_MarkMap *getMarkMap() {
		return _markMap;
	}
	void setMarkMap(MM_MarkMap *markMap) {
		_markMap = markMap;
	}

	static MM_ParallelHeapWalker *newInstance(MM_ParallelGlobalGC *globalCollector, MM_MarkMap *markMap, MM_EnvironmentBase *env); 	
	
	/**
	 * constructor of Parallel Heap Walker
	 */
	MM_ParallelHeapWalker(MM_ParallelGlobalGC *globalCollector, MM_MarkMap *markMap)
		: MM_HeapWalker()
		, _markMap(markMap)
		, _globalCollector(globalCollector)
	{
		_typeId = __FUNCTION__;
	}

	/*
	 * Friends
	 */
	friend class MM_ParallelObjectDoTask;
};

#endif /* PARALLEL_HEAP_WALKER_HPP_ */

