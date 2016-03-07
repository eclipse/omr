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
