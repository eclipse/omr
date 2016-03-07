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

#if !defined(CLASSUNLOADSTATS_HPP_)
#define CLASSUNLOADSTATS_HPP_

#include "Base.hpp"

#include "omrcomp.h"

/**
 * Storage for stats relevant to the class unloading phase of a collection.
 * @ingroup GC_Stats
 */
class MM_ClassUnloadStats : public MM_Base {
public:
	uintptr_t _classLoaderUnloadedCount; /**< number of unloaded class loaders */
	uintptr_t _classLoaderCandidates; /**< number of class loaders visited */
	uintptr_t _classesUnloadedCount; /**< number of unloaded classes */
	uintptr_t _anonymousClassesUnloadedCount; /**< number of anonymous classes unloaded */

	uint64_t _startTime; /**< Class unloading start time */
	uint64_t _endTime; /**< Class unloading end time */

	uint64_t _startSetupTime; /**< Class unloading setup start time */
	uint64_t _endSetupTime; /**< Class unloading setup end time */

	uint64_t _startScanTime; /**< Class unloading scan start time */
	uint64_t _endScanTime; /**< Class unloading scan end time */

	uint64_t _startPostTime; /**< Class unloading post-unloading start time */
	uint64_t _endPostTime; /**< Class unloading post-unloading end time */

	uint64_t _classUnloadMutexQuiesceTime; /**< Time the GC spends waiting on the classUnloadMutex */

	void clear();

	MM_ClassUnloadStats()
		: MM_Base()
		, _classLoaderUnloadedCount(0)
		, _classLoaderCandidates(0)
		, _classesUnloadedCount(0)
		, _anonymousClassesUnloadedCount(0)
		, _startTime(0)
		, _endTime(0)
		, _startSetupTime(0)
		, _endSetupTime(0)
		, _startScanTime(0)
		, _endScanTime(0)
		, _startPostTime(0)
		, _endPostTime(0)
		, _classUnloadMutexQuiesceTime(0) {};
};

#endif /* CLASSUNLOADSTATS_HPP_ */
