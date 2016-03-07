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

#if !defined(COLLECTIONSTATISTICS_HPP_)
#define COLLECTIONSTATISTICS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"

#include "Base.hpp"

/**
 * A collection of interesting statistics for the Heap.
 * @ingroup GC_Stats
 */
class MM_CollectionStatistics : public MM_Base
{
private:
protected:
public:
	uintptr_t _totalHeapSize; /**< Total active heap size */
	uintptr_t _totalFreeHeapSize; /**< Total free active heap */

	uint64_t  _startTime;		/**< Collection start time */
	uint64_t  _endTime;			/**< Collection end time */

	omrthread_process_time_t _startProcessTimes; /**< Process (Kernel and User) start time(s) */
	omrthread_process_time_t _endProcessTimes;   /**< Process (Kernel and User) end time(s) */
private:
protected:
public:

	/**
	 * Create a HeapStats object.
	 */
	MM_CollectionStatistics() :
		MM_Base()
		,_totalHeapSize(0)
		,_totalFreeHeapSize(0)
		,_startTime(0)
		,_endTime(0)
		,_startProcessTimes()
		,_endProcessTimes()
	{};
};

#endif /* COLLECTIONSTATISTICS_HPP_ */
