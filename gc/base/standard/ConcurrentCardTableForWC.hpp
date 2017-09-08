/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(CONCURRENTCARDTABLEFORWC_HPP_)
#define CONCURRENTCARDTABLEFORWC_HPP_


#include "omrcfg.h"

#include "Base.hpp"
#include "ConcurrentCardTable.hpp"
#include "EnvironmentStandard.hpp"
#include "ConcurrentSafepointCallback.hpp"


#define CARD_CLEAN_SAFE						(CARD_DIRTY << 7)

#define CONCURRENT_CARD_CLEAN_MASK_FOR_WC 	(CARD_CLEAN_SAFE | CARD_DIRTY)
#define FINAL_CARD_CLEAN_MASK_FOR_WC 		(CARD_DIRTY)
#define PREPARE_PARALLEL_MULTIPLIER			6 
//todo what is the best alignment for card cleaning chunks 
#define PREPARE_UNIT_SIZE_ALIGNMENT			128

typedef enum {
	MARK_DIRTY_CARD_SAFE=1,
	MARK_SAFE_CARD_DIRTY
} CardAction;

#if defined (OMR_GC_MODRON_CONCURRENT_MARK)

class MM_ConcurrentCardTableForWC : public MM_ConcurrentCardTable 
{
	bool _cardTablePreparedForCleaning;
	MM_ConcurrentSafepointCallback *_callback;
	
	bool initialize(MM_EnvironmentBase *env, MM_Heap *heap);
	virtual void tearDown(MM_EnvironmentBase *env);
		
	virtual	void prepareCardsForCleaning(MM_EnvironmentStandard *env);
	virtual bool getExclusiveCardTableAccess(MM_EnvironmentStandard *env, CardCleanPhase currentPhase, bool threadAtSafePoint);
	virtual void releaseExclusiveCardTableAccess(MM_EnvironmentStandard *env);
	void prepareCardTable(MM_EnvironmentStandard *env);
		
	uintptr_t countCardsInRange(MM_EnvironmentStandard *env, Card *rangeStart, Card *rangeEnd);
	
	MMINLINE virtual void concurrentCleanCard(Card *card)
	{
		/* Do nothing ...leave card ASIS, ie CARD_DIRTY or CARD_CLEAN_SAFE*/
		;	
	};
	
	MMINLINE virtual void finalCleanCard(Card *card)
	{
		*card = CARD_CLEAN;	
	};		
	
	
public:
	static MM_ConcurrentCardTable	*newInstance(MM_EnvironmentBase *env, MM_Heap *heap, MM_MarkingScheme *markingScheme, MM_ConcurrentGC *collector);
	
	void prepareCardTableChunk(MM_EnvironmentStandard *env, Card *chunkStart, Card *chunkEnd, CardAction action);
	virtual void initializeFinalCardCleaning(MM_EnvironmentStandard *env);
	
	static void prepareCardTableAsyncEventHandler(OMR_VMThread *omrVMThread, void *userData);
	
	/* TODO: For now always return false but we could just check if card is CARD_DIRTY 
	 * as we know such cards are sure to be cleaned in future either after a STW card table 
	 * prepare phase or during FCC.
	 */  
	MMINLINE virtual bool isObjectInUncleanedDirtyCard(MM_EnvironmentStandard *env, omrobjectptr_t object)
	{
		return false;	
	}
		
	/**
	 * Create a CardTableForWC object.
	 */
	MM_ConcurrentCardTableForWC(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme, MM_ConcurrentGC *collector):
		MM_ConcurrentCardTable(env, markingScheme, collector),
		_cardTablePreparedForCleaning(false)
		,_callback(NULL)
		{
			_typeId = __FUNCTION__;
		};
};

#endif /* defined (OMR_GC_MODRON_CONCURRENT_MARK)*/

#endif /* CONCURRENTCARDTABLEFORWC_HPP_ */
