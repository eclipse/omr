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

#if !defined(GLOBALALLOCATIONMANAGERSEGREGATED_HPP_)
#define GLOBALALLOCATIONMANAGERSEGREGATED_HPP_

#include "omrcfg.h"

#include "GlobalAllocationManager.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_AllocationContextSegregated;
class MM_EnvironmentBase;
class MM_RegionPoolSegregated;
class MM_SegregatedMarkingScheme;
class MM_SweepSchemeSegregated;

class MM_GlobalAllocationManagerSegregated : public MM_GlobalAllocationManager
{
	/*
	 * Data members
	 */
private:
	MM_RegionPoolSegregated *_regionPool;
protected:
public:

	/*
	 * Function members
	 */
private:
protected:
	bool initialize(MM_EnvironmentBase *env, MM_RegionPoolSegregated *regionPool);
	virtual void tearDown(MM_EnvironmentBase *env);
	MM_GlobalAllocationManagerSegregated(MM_EnvironmentBase *env)
		: MM_GlobalAllocationManager(env)
		, _regionPool(NULL)
	{
		_typeId = __FUNCTION__;
	};

	/**
	 * Create a single allocation context associated with the receiver
	 */
	virtual MM_AllocationContextSegregated * createAllocationContext(MM_EnvironmentBase * env, MM_RegionPoolSegregated *regionPool);

	/**
	 * One time initialization of the allocation contexts, occurs after GAM initialization since it's
	 * called by the MPS immediately after the region table is created.
	 */
	virtual bool initializeAllocationContexts(MM_EnvironmentBase * env, MM_RegionPoolSegregated *regionPool);

public:
	static MM_GlobalAllocationManagerSegregated *newInstance(MM_EnvironmentBase *env, MM_RegionPoolSegregated *regionPool);
	virtual void kill(MM_EnvironmentBase *env);

	virtual bool acquireAllocationContext(MM_EnvironmentBase *env);
	virtual void releaseAllocationContext(MM_EnvironmentBase *env);
	virtual void printAllocationContextStats(MM_EnvironmentBase *env, uintptr_t eventNum, J9HookInterface** hookInterface) { /* no impl */ }

	/**
	 * Set the sweep scheme to use when sweeping a region
	 */
	void setSweepScheme(MM_SweepSchemeSegregated *sweepScheme);
	
	/**
	 * Set the marking scheme to use when AllocationContext premarks cells
	 */
	void setMarkingScheme(MM_SegregatedMarkingScheme *markingScheme);

	/**
	 * flush each context's full region queues to the region pool
	 */
	void flushCachedFullRegions(MM_EnvironmentBase *env);

	MM_RegionPoolSegregated *getRegionPool() { return _regionPool; }

};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* GLOBALALLOCATIONMANAGERSEGREGATED_HPP_ */
