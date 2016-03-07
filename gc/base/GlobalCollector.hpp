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
 ******************************************************************************/

#if !defined(GLOBALCOLLECTOR_HPP_)
#define GLOBALCOLLECTOR_HPP_

#include "omrcomp.h"
#include "omrgcconsts.h"
#include "modronbase.h"
#include "omr.h"

#include "Collector.hpp"

class MM_EnvironmentBase;
class MM_MemorySubSpace;

/**
 * Abstract class representing a collector for the entire heap.
 * @ingroup GC_Base_Core
 */
class MM_GlobalCollector : public MM_Collector {
private:
protected:
	virtual void internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace);

public:
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

	MM_GlobalCollector(MM_EnvironmentBase* env, MM_CollectorLanguageInterface *cli)
		: MM_Collector(cli)
	{
		_typeId = __FUNCTION__;
		_cycleType = OMR_GC_CYCLE_TYPE_GLOBAL;
	}
};

#endif /* GLOBALCOLLECTOR_HPP_ */
