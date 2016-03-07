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

#if !defined(CARDCLEANINGSTATS_HPP_)
#define CARDCLEANINGSTATS_HPP_

#include "omrcomp.h"
#include "modronbase.h"

#include "Base.hpp"

class MM_CardCleaningStats : public MM_Base
{
/* Data Members */
public:
	uint64_t _cardCleaningTime; /**< Time spent cleaning cards in hi-res clock resolution. */
	uintptr_t _cardsCleaned; /**< The number of cards cleaned */
	
/* Function Members */
public:
	/**
	 * Reset the card cleaning statistics to their initial state.  Statistics should
	 * be reset each for each local or global GC.
	 */
	void clear();
	
	/**
	 * Add the specified interval to the amount of time attributed to card cleaning.
	 * @param startTime The time scanning began, measured by omrtime_hires_clock()
	 * @param endTime The time scanning ended, measured by omrtime_hires_clock()
	 */
	MMINLINE void addToCardCleaningTime(uint64_t startTime, uint64_t endTime) { _cardCleaningTime += (endTime - startTime);	}
	
	/**
	 * Merges the results from the input MM_CardCleaningStats with the statistics contained within the receiver.
	 * 
	 * @param[in] statsToMerge	Card cleaning statistics to merge with the receiver
	 */
	void merge(MM_CardCleaningStats *statsToMerge);
	
	MM_CardCleaningStats() :
		MM_Base()
	{
		clear();
	}
};

#endif /* !CARDCLEANINGSTATS_HPP_ */
