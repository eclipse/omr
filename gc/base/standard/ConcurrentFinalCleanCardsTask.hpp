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
 * @ingroup GC_Modron_Standard
 */

#if !defined(CONCURRENTFINALCLEANCARDSTASK_HPP_)
#define CONCURRENTFINALCLEANCARDSTASK_HPP_

#include "omrcfg.h"
#include "omrmodroncore.h"

#include "CycleState.hpp"
#include "ParallelTask.hpp"

class MM_ConcurrentGCIncrementalUpdate;
class MM_EnvironmentBase;
class MM_ParallelDispatcher;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ConcurrentFinalCleanCardsTask : public MM_ParallelTask
{
private:
	MM_ConcurrentGCIncrementalUpdate *_collector;
	MM_CycleState *_cycleState;  /**< Collection cycle state active for the task */

public:
	virtual UDATA getVMStateID() { return OMRVMSTATE_GC_CONCURRENT_MARK_FINAL_CLEAN_CARDS; };
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);

	/**
	 * Create a ConcurrentFinalCleanCardsTask object
	 */
	MM_ConcurrentFinalCleanCardsTask(MM_EnvironmentBase *env, MM_ParallelDispatcher *dispatcher, MM_ConcurrentGCIncrementalUpdate *collector, MM_CycleState *cycleState) :
		MM_ParallelTask(env, dispatcher)
		,_collector(collector)
		,_cycleState(cycleState)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* CONCURRENTFINALCLEANCARDSTASK_HPP_ */
