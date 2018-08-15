/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(EVACUATORPARALLELTASK_HPP_)
#define EVACUATORPARALLELTASK_HPP_

#include "omrcfg.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "modronopt.h"
#include "omrmodroncore.h"

#include "CycleState.hpp"
#include "ParallelTask.hpp"

class MM_Dispatcher;
class MM_EnvironmentBase;
class MM_EvacuatorController;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_EvacuatorParallelTask : public MM_ParallelTask
{
protected:
	MM_EvacuatorController *_controller;
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
	MM_EvacuatorParallelTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_EvacuatorController *controller, MM_CycleState *cycleState)
		: MM_ParallelTask(env, dispatcher)
		, _controller(controller)
		, _cycleState(cycleState)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#endif /* EVACUATORPARALLELTASK_HPP_ */
