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

#if !defined(METRONOMESTATS_HPP_)
#define METRONOMESTATS_HPP_

#include "Base.hpp"
#include "AtomicOperations.hpp"

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
