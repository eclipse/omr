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
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(CONCURRENTGMPSTATS_HPP_)
#define CONCURRENTGMPSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"

#if defined(OMR_GC_VLHGC)

#include "Base.hpp"

/**
 * Stats structure intended to be used by the Concurrent GMP triggers (firstly, for verbose) in VLHGC.
 * @ingroup GC_Stats
 */
class MM_ConcurrentGMPStats : public MM_Base
{
	/* Data Members */
private:
protected:
public:
	uintptr_t _gmpCycleID;	/**< The "_id" of the corresponding GMP cycle (since concurrent operations are always logically part of a GMP cycle) */
	uintptr_t _scanTargetInBytes;	/**< The number of bytes a given concurrent task was expected to scan before terminating */
	uintptr_t _bytesScanned;	/**< The number of bytes a given concurrent task did scan before it terminated (can be lower than _scanTargetInBytes if the termination was asynchronously requested) */
	bool _terminationWasRequested;	/**< True if a given concurrent task's termination was asynchronously requested (although it might have terminated due to meeting its target, anyway - there is a timing window which allows both of these reasons to be true) */

	/* Member Functions */
private:
protected:
public:
	MM_ConcurrentGMPStats()
		: MM_Base()
		, _gmpCycleID(0)
		, _scanTargetInBytes(0)
		, _bytesScanned(0)
		, _terminationWasRequested(false)
	{}
}; 

#endif /* defined(OMR_GC_VLHGC) */

#endif /* CONCURRENTGMPSTATS_HPP_ */
