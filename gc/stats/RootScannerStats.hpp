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
	uint64_t _entityScanTime[RootScannerEntity_Count]; /**< Time spent scanning each root scanner entity per thread.  Values of 0 indicate no time (regardless of clock resolution) spent scanning. */
	
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
