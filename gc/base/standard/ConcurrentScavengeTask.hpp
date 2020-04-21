/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

#if !defined(GC_BASE_STANDARD_CONCURRENTSCAVENGETASK_HPP_)
#define GC_BASE_STANDARD_CONCURRENTSCAVENGETASK_HPP_

#include "omrcfg.h"

#if defined(OMR_GC_CONCURRENT_SCAVENGER)

#include "modronopt.h"

#include "ParallelScavengeTask.hpp"

class MM_ConcurrentScavengeTask : public MM_ParallelScavengeTask
{
	/* Data Members */
private:
	volatile uintptr_t _bytesScanned;	/**< The number of bytes scanned by this */
protected:
public:

	enum ConcurrentAction {
		SCAVENGE_ALL = 1,
		SCAVENGE_ROOTS,
		SCAVENGE_SCAN,
		SCAVENGE_COMPLETE
	};

	/* _action should be private */
	ConcurrentAction _action;

	/* Member Functions */
private:
protected:
public:
	virtual uintptr_t getVMStateID()
	{
		return OMRVMSTATE_GC_CONCURRENT_SCAVENGER;
	}

	uintptr_t getBytesScanned()
	{
		return _bytesScanned;
	}
	virtual void run(MM_EnvironmentBase *env);

	MM_ConcurrentScavengeTask(MM_EnvironmentBase *env,
			MM_Dispatcher *dispatcher,
			MM_Scavenger *scavenger,
			ConcurrentAction action,
			MM_CycleState *cycleState) :
		MM_ParallelScavengeTask(env, dispatcher, scavenger, cycleState)
		, _bytesScanned(0)
		, _action(action)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* OMR_GC_CONCURRENT_SCAVENGER */
#endif /* GC_BASE_STANDARD_CONCURRENTSCAVENGETASK_HPP_ */
