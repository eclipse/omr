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

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(IRRSSTATS_HPP_)
#define IRRSSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrport.h"
//#include "j9consts.h"
//#include "modronopt.h"

#include "Base.hpp"

/**
 * Storage for statistics relevant to the sweep phase of global collection
 * @ingroup GC_Stats
 */
class MM_InterRegionRememberedSetStats : public MM_Base
{
public:

	uint64_t _rebuildCompressedCardTableTimesus;		/**< time to do compress Card Table */
	uint64_t _clearFromRegionReferencesTimesus;			/**< time to do clear-from-CS-region pass */
	uintptr_t _clearFromRegionReferencesCardsProcessed; /**< references (cards) visited during clear-from-CS-region pass (effectively all cards from all RSCLs) */
	uintptr_t _clearFromRegionReferencesCardsCleared;	/**< count of references removed during the pass */

	void clear() {
		_rebuildCompressedCardTableTimesus = 0;
		_clearFromRegionReferencesTimesus = 0;
		_clearFromRegionReferencesCardsProcessed = 0;
		_clearFromRegionReferencesCardsCleared = 0;
	}
	
	void merge(MM_InterRegionRememberedSetStats *statsToMerge) {
		/* we cannot precisely measure the total duration, so we'll approximate with the longest duration  */
		_rebuildCompressedCardTableTimesus = OMR_MAX(_rebuildCompressedCardTableTimesus, statsToMerge->_rebuildCompressedCardTableTimesus);
		_clearFromRegionReferencesTimesus = OMR_MAX(_clearFromRegionReferencesTimesus, statsToMerge->_clearFromRegionReferencesTimesus);

		_clearFromRegionReferencesCardsProcessed += statsToMerge->_clearFromRegionReferencesCardsProcessed;
		_clearFromRegionReferencesCardsCleared += statsToMerge->_clearFromRegionReferencesCardsCleared;
	}

	MM_InterRegionRememberedSetStats() :
		MM_Base()
	{
		clear();
	}
};

#endif /* IRRSSTATS_HPP_ */
