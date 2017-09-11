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

#if !defined(SEGREGATEDALLOCATIONTRACKER_HPP_)
#define SEGREGATEDALLOCATIONTRACKER_HPP_

#include "omrcomp.h"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_EnvironmentBase;

class MM_SegregatedAllocationTracker : public MM_BaseVirtual
{
public:
protected:
private: 
	intptr_t _bytesAllocated; /**< A negative amount indicates this tracker has freed more bytes than allocated. */
	uintptr_t _flushThreshold; /**< If |bytesAllocated| > this threshold, we'll flush the bytes allocated to the pool. */
	volatile uintptr_t *_globalBytesInUse; /**< The memory pool accumulator to flush bytes to */

public:
	static MM_SegregatedAllocationTracker* newInstance(MM_EnvironmentBase *env, volatile uintptr_t *globalBytesInUse, uintptr_t flushThreshold);
	virtual void kill(MM_EnvironmentBase *env);

	static void updateAllocationTrackerThreshold(MM_EnvironmentBase* env);
	static void initializeGlobalAllocationTrackerValues(MM_EnvironmentBase* env);
	
	void addBytesAllocated(MM_EnvironmentBase* env, uintptr_t bytesAllocated);
	void addBytesFreed(MM_EnvironmentBase* env, uintptr_t bytesFreed);
	intptr_t getUnflushedBytesAllocated(MM_EnvironmentBase* env) { return _bytesAllocated; }
	
protected:
	virtual bool initialize(MM_EnvironmentBase *env, uintptr_t volatile *globalBytesInUse, uintptr_t flushThreshold);
	virtual void tearDown(MM_EnvironmentBase *env);
	
private:
	MM_SegregatedAllocationTracker(MM_EnvironmentBase *env) :
		_bytesAllocated(0)
		,_flushThreshold(0)
		,_globalBytesInUse(NULL)
	{
		_typeId = __FUNCTION__;
	};

	void flushBytes();
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /*SEGREGATEDALLOCATIONTRACKER_HPP_*/
