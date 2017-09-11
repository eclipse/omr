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

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(CONCURRENTSCANREMEMBERSETTASK_HPP_)
#define CONCURRENTSCANREMEMBERSETTASK_HPP_

#include "omrcfg.h"
#include "omrmodroncore.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "CycleState.hpp"
#include "ParallelTask.hpp"

class MM_ConcurrentGC;
class MM_Dispatcher;
class MM_EnvironmentBase;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ConcurrentScanRememberedSetTask : public MM_ParallelTask
{
private:
	MM_ConcurrentGC *_collector;
	MM_CycleState *_cycleState;  /**< Collection cycle state active for the task */

public:
	virtual UDATA getVMStateID() { return J9VMSTATE_GC_CONCURRENT_MARK_SCAN_REMEMBERED_SET; };
	
	virtual void run(MM_EnvironmentBase *envBase);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);

	/**
	 * Create a ConcurrentScanRememberedSetTask object
	 */
	MM_ConcurrentScanRememberedSetTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_ConcurrentGC *collector, MM_CycleState *cycleState) :
		MM_ParallelTask(env, dispatcher)
		,_collector(collector)
		,_cycleState(cycleState)
	{
		_typeId = __FUNCTION__;
	};
};
#endif /* OMR_GC_MODRON_SCAVENGER */
#endif /* CONCURRENTSCANREMEMBERSETTASK_HPP_ */
