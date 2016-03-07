/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(CONCURRENTFINALCLEANCARDSTASK_HPP_)
#define CONCURRENTFINALCLEANCARDSTASK_HPP_

#include "omrcfg.h"
#include "omrmodroncore.h"

#include "CycleState.hpp"
#include "ParallelTask.hpp"

class MM_ConcurrentGC;
class MM_Dispatcher;
class MM_EnvironmentBase;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ConcurrentFinalCleanCardsTask : public MM_ParallelTask
{
private:
	MM_ConcurrentGC *_collector;
	MM_CycleState *_cycleState;  /**< Collection cycle state active for the task */

public:
	virtual UDATA getVMStateID() { return J9VMSTATE_GC_CONCURRENT_MARK_FINAL_CLEAN_CARDS; };
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);

	/**
	 * Create a ConcurrentFinalCleanCardsTask object
	 */
	MM_ConcurrentFinalCleanCardsTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_ConcurrentGC *collector, MM_CycleState *cycleState) :
		MM_ParallelTask(env, dispatcher)
		,_collector(collector)
		,_cycleState(cycleState)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* CONCURRENTFINALCLEANCARDSTASK_HPP_ */
