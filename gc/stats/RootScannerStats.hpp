/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(ROOTSCANNERSTATS_HPP_)
#define ROOTSCANNERSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"

#include "Base.hpp"
#include "RootScannerTypes.h"

class MM_RootScannerStats : public MM_Base
{
/* Data Members */
public:
	bool _statsUsed; /**< Flag that indicates if the owner thread used the stats for last increment (for any of its roots) */
	uint64_t _entityScanTime[RootScannerEntity_Count]; /**< Time spent scanning each root scanner entity per thread.  Values of 0 indicate no time (regardless of clock resolution) spent scanning. */
	uint64_t _maxIncrementTime;  /**< Longest increment */
	RootScannerEntity _maxIncrementEntity; /**< Entity of the longest increment */
	
/* Function Members */
public:
	/**
	 * Reset the root scanner statistics to their initial state.  Statistics should
	 * be reset each for each local or global GC.
	 */
	void clear();
	
	/**
	 * Merges the results from the input MM_RootScannerStats with the statistics contained within
	 * the instance.
	 * 
	 * @param[in] statsToMerge	Root scanner statistics
	 */
	void merge(MM_RootScannerStats *statsToMerge);
	
	MM_RootScannerStats() :
		MM_Base()
	{
		clear();
	};
};

#endif /* !ROOTSCANNERSTATS_HPP_ */
