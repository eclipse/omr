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
