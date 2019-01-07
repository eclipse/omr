/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#if !defined(METRONOMESTATS_HPP_)
#define METRONOMESTATS_HPP_

#include "Base.hpp"
#include "AtomicOperations.hpp"
#include "ReferenceStats.hpp"

/**
 * @todo Provide class documentation
 * Stats collected during one GC interval (quantum)
 * @ingroup GC_Stats
 */
class MM_MetronomeStats : public MM_Base {
public:
	uintptr_t classLoaderUnloadedCount;
	uintptr_t classesUnloadedCount;
	uintptr_t anonymousClassesUnloadedCount;

	uintptr_t finalizableCount; /**< count of objects pushed for finalization during one quantum */

	uintptr_t _workPacketOverflowCount; /**< count of work packets overflowed since the end of the last quantum */
	uintptr_t _objectOverflowCount; /**< count of single objects that are overflowed since the last quantum */

	uintptr_t nonDeterministicSweepCount;
	uintptr_t nonDeterministicSweepConsecutive;
	uint64_t nonDeterministicSweepDelay;

	uint64_t _microsToStopMutators; /**< The number of microseconds the master thread had to wait for the mutator threads to stop, at the beginning of this increment */

	UDATA _unfinalizedCandidates; /**< unfinalized objects that are candidates to be finalized visited this cycle */
	UDATA _unfinalizedEnqueued; /**< unfinalized objects that are enqueued during this cycle (MUST be less than or equal _unfinalizedCandidates) */

	MM_ReferenceStats _weakReferenceStats; /**< Weak reference stats for the cycle */
	MM_ReferenceStats _softReferenceStats; /**< Soft reference stats for the cycle */
	MM_ReferenceStats _phantomReferenceStats; /**< Phantom reference stats for the cycle */
protected:
private:
public:
	/**
	 * To be called at the begining of a GC interval (quantum).
	 */
	void clearStart()
	{
		classLoaderUnloadedCount = 0;
		classesUnloadedCount = 0;
		anonymousClassesUnloadedCount = 0;
		finalizableCount = 0;
	}

	/**
	 * To be called at the end of a GC interval (quantum).
	 */
	void clearEnd()
	{
		nonDeterministicSweepCount = 0;
		nonDeterministicSweepConsecutive = 0;
		nonDeterministicSweepDelay = 0;
		_workPacketOverflowCount = 0;
		_objectOverflowCount = 0;
		_microsToStopMutators = 0;
		_weakReferenceStats.clear();
		_softReferenceStats.clear();
		_phantomReferenceStats.clear();
	}

	MMINLINE void incrementWorkPacketOverflowCount()
	{
		MM_AtomicOperations::add(&_workPacketOverflowCount, 1);
	}

	MMINLINE void incrementObjectOverflowCount()
	{
		MM_AtomicOperations::add(&_objectOverflowCount, 1);
	}

	MMINLINE uintptr_t getWorkPacketOverflowCount()
	{
		return _workPacketOverflowCount;
	}

	MMINLINE uintptr_t getObjectOverflowCount()
	{
		return _objectOverflowCount;
	}

	void merge(MM_MetronomeStats* statsToMerge);

	/**
	 * Create a MetronomeStats object.
	 */
	MM_MetronomeStats()
		: MM_Base()
		, classLoaderUnloadedCount(0)
		, classesUnloadedCount(0)
		, anonymousClassesUnloadedCount(0)
		, finalizableCount(0)
		, _workPacketOverflowCount(0)
		, _objectOverflowCount(0)
		, nonDeterministicSweepCount(0)
		, nonDeterministicSweepConsecutive(0)
		, nonDeterministicSweepDelay(0)
		, _microsToStopMutators(0)
	{
	}

protected:
private:
};

#endif /* METRONOMESTATS_HPP_ */
