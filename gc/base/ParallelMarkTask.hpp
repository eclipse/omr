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

#if !defined(PARALLELMARKTASK_HPP_)
#define PARALLELMARKTASK_HPP_

#include "omrcfg.h"

#include "CycleState.hpp"
#include "ParallelTask.hpp"

class MM_Dispatcher;
class MM_EnvironmentBase;
class MM_MarkingScheme;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ParallelMarkTask : public MM_ParallelTask
{
private:
	MM_MarkingScheme *_markingScheme;
	const bool _initMarkMap;
	MM_CycleState *_cycleState;  /**< Collection cycle state active for the task */
	
public:
	virtual uintptr_t getVMStateID();
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);
	
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	virtual void synchronizeGCThreads(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id);
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	/**
	 * Create a ParallelMarkTask object.
	 */
	MM_ParallelMarkTask(MM_EnvironmentBase *env,
			MM_Dispatcher *dispatcher, 
			MM_MarkingScheme *markingScheme, 
			bool initMarkMap,
			MM_CycleState *cycleState) :
		MM_ParallelTask(env, dispatcher)
		,_markingScheme(markingScheme)
		,_initMarkMap(initMarkMap)
		,_cycleState(cycleState)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* PARALLELMARKTASK_HPP_ */
