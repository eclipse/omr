/*******************************************************************************
 * Copyright (c) 2015 IBM Corporation
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *    IBM Corporation - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(GC_BASE_STANDARD_PARALLELSCAVENGETASK_HPP_)
#define GC_BASE_STANDARD_PARALLELSCAVENGETASK_HPP_

#include "omrcfg.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "modronopt.h"
#include "omr/gc/modronCore.h"

#include "CycleState.hpp"
#include "ParallelTask.hpp"

class MM_Dispatcher;
class MM_EnvironmentBase;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ParallelScavengeTask : public MM_ParallelTask
{
protected:
	MM_Scavenger *_collector;
	MM_CycleState *_cycleState;  /**< Collection cycle state active for the task */

public:
	virtual UDATA getVMStateID() { return J9VMSTATE_GC_SCAVENGE; };
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	/**
	 * Override to collect stall time statistics.
	 * @see MM_ParallelTask::synchronizeGCThreads
	 */
	virtual void synchronizeGCThreads(MM_EnvironmentBase *env, const char *id);

	/**
	 * Override to collect stall time statistics.
	 * @see MM_ParallelTask::synchronizeGCThreadsAndReleaseMaster
	 */
	virtual bool synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id);
	
	/**
	 * Override to collect stall time statistics.
	 * @see MM_ParallelTask::synchronizeGCThreadsAndReleaseSingleThread
	 */
	virtual bool synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id);
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	/**
	 * Create a ParallelScavengeTask object.
	 */
	MM_ParallelScavengeTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_Scavenger *collector,MM_CycleState *cycleState) :
		MM_ParallelTask(env, dispatcher)
		,_collector(collector)
		,_cycleState(cycleState)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#endif /* GC_BASE_STANDARD_PARALLELSCAVENGETASK_HPP_ */
