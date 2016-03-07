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
 ******************************************************************************/

/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(DISPATCHER_HPP_)
#define DISPATCHER_HPP_

#include "omrcfg.h"

#include "modronbase.h"

#include "BaseVirtual.hpp"

class MM_EnvironmentBase;
class MM_Task;

/**
 * @todo Provide define documentation
 * @ingroup GC_Base_Core
 */
#define J9MODRON_HANDLE_NEXT_WORK_UNIT(envPtr) envPtr->_currentTask->handleNextWorkUnit(envPtr)

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_Dispatcher : public MM_BaseVirtual
{
private:
	MM_Task *_task;
	
protected:
	bool initialize(MM_EnvironmentBase *env);

	virtual void prepareThreadsForTask(MM_EnvironmentBase *env, MM_Task *task);
	virtual void acceptTask(MM_EnvironmentBase *env);
	virtual void completeTask(MM_EnvironmentBase *env);
	virtual void cleanupAfterTask(MM_EnvironmentBase *env);

public:
	static MM_Dispatcher *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	virtual bool startUpThreads();
	virtual void shutDownThreads();

	virtual bool condYieldFromGCWrapper(MM_EnvironmentBase *env, uint64_t timeSlack = 0) { return false; }

	MMINLINE virtual uintptr_t threadCount() { return 1; }
	MMINLINE virtual uintptr_t threadCountMaximum() { return 1; }
	MMINLINE virtual uintptr_t activeThreadCount() { return 1; }

	void run(MM_EnvironmentBase *env, MM_Task *task);
	virtual void reinitAfterFork(MM_EnvironmentBase *env, uintptr_t newThreadCount) {}

	/**
	 * Create a Dispatcher object.
	 */
	MM_Dispatcher(MM_EnvironmentBase *env) :
		MM_BaseVirtual(),
		_task(NULL)
	{
		_typeId = __FUNCTION__;
	};
	
	friend class MM_Task;
};

#endif /* DISPATCHER_HPP_ */
