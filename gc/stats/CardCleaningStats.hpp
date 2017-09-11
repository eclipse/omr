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
