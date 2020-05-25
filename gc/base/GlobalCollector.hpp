/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#if !defined(GLOBALCOLLECTOR_HPP_)
#define GLOBALCOLLECTOR_HPP_

#include "omrcomp.h"
#include "omrgcconsts.h"
#include "modronbase.h"
#include "omr.h"

#include "Collector.hpp"
#include "GlobalCollectorDelegate.hpp"

class MM_EnvironmentBase;
class MM_MemorySubSpace;

/**
 * Abstract class representing a collector for the entire heap.
 * @ingroup GC_Base_Core
 */
class MM_GlobalCollector : public MM_Collector {
private:
protected:
	MM_GlobalCollectorDelegate _delegate; /**< Language specific delegate -- subclass must initialize */

public:

private:
protected:
	virtual void internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace);

public:
	MM_GlobalCollectorDelegate *getGlobalCollectorDelegate() { return &_delegate; }


	/*
	 * Determine if a collector (some parent, typically the global collector) wishes to usurp any minor collection.
	 * @return boolean indicating if the parent collector should be invoked in place of a child.
	 */
	virtual bool isTimeForGlobalGCKickoff();

	virtual bool condYield(MM_EnvironmentBase *env, uint64_t timeSlackNanoSec)
	{
		return false;
	}

	virtual bool shouldYield(MM_EnvironmentBase *env)
	{
		return false;
	}

	virtual void yield(MM_EnvironmentBase *env) {};

	/**
 	 * Perform any collector-specific initialization.
 	 * @return TRUE if startup completes OK, FALSE otherwise
 	 */
	virtual bool collectorStartup(MM_GCExtensionsBase* extensions) = 0;

	/**
	 * Perform any collector-specific shutdown actions.
	 */
	virtual void collectorShutdown(MM_GCExtensionsBase* extensions) = 0;

	/**
	 * Abort any currently active garbage collection activity.
	 */
	virtual void abortCollection(MM_EnvironmentBase* env, CollectionAbortReason reason);

	/**
 	* Abstract for request to create sweepPoolState class for pool
 	* @param  memoryPool memory pool to attach sweep state to
 	* @return pointer to created class
 	*/
	virtual void* createSweepPoolState(MM_EnvironmentBase* env, MM_MemoryPool* memoryPool) = 0;

	/**
 	* Abstract for request to destroy sweepPoolState class for pool
 	* @param  sweepPoolState class to destroy
 	*/
	virtual void deleteSweepPoolState(MM_EnvironmentBase* env, void* sweepPoolState) = 0;

	MM_GlobalCollector()
		: MM_Collector()
		, _delegate()
	{
		_typeId = __FUNCTION__;
		_cycleType = OMR_GC_CYCLE_TYPE_GLOBAL;
	}
};

#endif /* GLOBALCOLLECTOR_HPP_ */
