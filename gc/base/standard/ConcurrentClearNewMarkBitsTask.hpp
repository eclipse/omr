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

#if !defined(CONCURRENTCLEARNEWMARKBITSTASK_HPP_)
#define CONCURRENTCLEARNEWMARKBITSTASK_HPP_

#include "omrcfg.h"
#include "omrmodroncore.h"

#include "ParallelTask.hpp"

class MM_ConcurrentGC;
class MM_Dispatcher;
class MM_EnvironmentBase;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ConcurrentClearNewMarkBitsTask : public MM_ParallelTask
{
private:
	MM_ConcurrentGC *_collector;

public:
	virtual UDATA getVMStateID() { return J9VMSTATE_GC_CONCURRENT_MARK_CLEAR_NEW_MARKBITS; };
	
	virtual void run(MM_EnvironmentBase *env);

	/**
	 * Create a ConcurrentClearNewMarkBitsTask object
	 */
	MM_ConcurrentClearNewMarkBitsTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_ConcurrentGC *collector) :
		MM_ParallelTask(env, dispatcher),
		_collector(collector)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* CONCURRENTCLEARNEWMARKBITSTASK_HPP_ */
