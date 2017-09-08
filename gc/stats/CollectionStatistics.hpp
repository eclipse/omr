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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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
