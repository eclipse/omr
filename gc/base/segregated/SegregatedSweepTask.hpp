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

#if !defined(SEGREGATEDSWEEPTASK_HPP_)
#define SEGREGATEDSWEEPTASK_HPP_

#include "omrmodroncore.h"

#include "ParallelTask.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_SegregatedSweepTask : public MM_ParallelTask
{
/* Data members / types */
public:
protected:
private:
	MM_SweepSchemeSegregated *_sweepScheme;
	MM_MemoryPoolSegregated *_memoryPool;

/* Methods */
public:
	/* OMRTODO come up with a better number here.. */
	virtual uintptr_t getVMStateID() { return J9VMSTATE_GC_SWEEP; };
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);
	
	MM_SegregatedSweepTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_SweepSchemeSegregated *sweepScheme, MM_MemoryPoolSegregated *memoryPool)
		: MM_ParallelTask(env, dispatcher)
		, _sweepScheme(sweepScheme)
		, _memoryPool(memoryPool)
	{
		_typeId = __FUNCTION__;
	}
protected:
private:
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* SEGREGATEDSWEEPTASK_HPP_ */
