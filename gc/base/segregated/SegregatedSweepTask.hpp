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
