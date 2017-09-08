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

#if !defined(CARDTABLESTATS_HPP_)
#define CARDTABLESTATS_HPP_

#include "omrcomp.h"
#include "modronbase.h"

#include "AtomicOperations.hpp"
#include "Base.hpp"

#define HIGH_VALUES (uintptr_t)(-1)
/**
 * @todo Provide class documentation
 * @ingroup GC_Stats
 */
class MM_ConcurrentCardTableStats : public MM_Base 
{
public:
	uintptr_t totalCards; 
	uintptr_t _cardCleaningPhase1Kickoff;
	uintptr_t _cardCleaningPhase2Kickoff;
	uintptr_t _cardCleaningPhase3Kickoff;
	
	volatile uintptr_t concurrentCleanedCardsPhase1;
	volatile uintptr_t finalCleanedCardsPhase1;

	volatile uintptr_t concurrentCleanedCardsPhase2;
	volatile uintptr_t finalCleanedCardsPhase2;
	
	volatile uintptr_t concurrentCleanedCardsPhase3;
	
	MMINLINE void setCount(volatile uintptr_t &counter, uintptr_t count) 
	{ 
		MM_AtomicOperations::set((uintptr_t *)&counter,(uintptr_t)count);
	};
	
	MMINLINE void incrementCount(volatile uintptr_t &counter, uintptr_t count) 
	{ 
		MM_AtomicOperations::add((uintptr_t *)&counter,(uintptr_t)count);
	};
	
	MMINLINE void initializeCardCleaningStatistics()
	{
		/* Concurrent phase kickoff points */
		_cardCleaningPhase1Kickoff = HIGH_VALUES;
		_cardCleaningPhase2Kickoff = HIGH_VALUES;
		_cardCleaningPhase3Kickoff = HIGH_VALUES;
		
		/* Concurrent card cleaning counts */
		setCount(concurrentCleanedCardsPhase1, 0);
		setCount(concurrentCleanedCardsPhase2, 0);
		setCount(concurrentCleanedCardsPhase3, 0);
		
		/* Final card cleaning counts */
		setCount(finalCleanedCardsPhase1, 0);
		setCount(finalCleanedCardsPhase2, 0);
	}
	
	MMINLINE void setCardCleaningPhase1Kickoff(uintptr_t kickoff) { _cardCleaningPhase1Kickoff = kickoff; };
	MMINLINE uintptr_t getCardCleaningPhase1Kickoff() { return _cardCleaningPhase1Kickoff; };
	
	MMINLINE void setCardCleaningPhase2Kickoff(uintptr_t kickoff) { _cardCleaningPhase2Kickoff = kickoff; };
	MMINLINE uintptr_t getCardCleaningPhase2Kickoff() { return _cardCleaningPhase2Kickoff; };
	
	MMINLINE void setCardCleaningPhase3Kickoff(uintptr_t kickoff) { _cardCleaningPhase3Kickoff = kickoff; };
	MMINLINE uintptr_t getCardCleaningPhase3Kickoff() { return _cardCleaningPhase3Kickoff; };
	
	MMINLINE uintptr_t getConcurrentCleanedCards(){ return concurrentCleanedCardsPhase1 + concurrentCleanedCardsPhase2 + concurrentCleanedCardsPhase3; };
	MMINLINE uintptr_t getFinalCleanedCards(){ return finalCleanedCardsPhase1 + finalCleanedCardsPhase2; };
	
	MMINLINE uintptr_t getConcurrentCleanedCardsPhase1() { return concurrentCleanedCardsPhase1; };
	MMINLINE void incConcurrentCleanedCardsPhase1(uintptr_t numCards)
	{
		incrementCount(concurrentCleanedCardsPhase1, numCards);
	};
	
	MMINLINE uintptr_t getConcurrentCleanedCardsPhase2() { return concurrentCleanedCardsPhase2; };
	MMINLINE void incConcurrentCleanedCardsPhase2(uintptr_t numCards)
	{
		incrementCount(concurrentCleanedCardsPhase2, numCards);
	};
	
	MMINLINE uintptr_t getConcurrentCleanedCardsPhase3() { return concurrentCleanedCardsPhase3; };
	MMINLINE void incConcurrentCleanedCardsPhase3(uintptr_t numCards)
	{
		incrementCount(concurrentCleanedCardsPhase3, numCards);
	};
	
	MMINLINE uintptr_t getFinalCleanedCardsPhase1() { return finalCleanedCardsPhase1; };
	MMINLINE void incFinalCleanedCardsPhase1(uintptr_t numCards)
	{
		incrementCount(finalCleanedCardsPhase1, numCards);	
	};
		
	MMINLINE uintptr_t getFinalCleanedCardsPhase2() { return finalCleanedCardsPhase2; };
	MMINLINE void incFinalCleanedCardsPhase2(uintptr_t numCards)
	{
		incrementCount(finalCleanedCardsPhase2, numCards);	
	};
	
	/**
	 * Create a CardTableStats object.
	 */   
	MM_ConcurrentCardTableStats() :
		MM_Base(),
		totalCards(0),
		concurrentCleanedCardsPhase1(0),
		finalCleanedCardsPhase1(0),
		concurrentCleanedCardsPhase2(0),
		finalCleanedCardsPhase2(0),
		concurrentCleanedCardsPhase3(0)
	{};
};

#endif /* CARDTABLESTATS_HPP_ */
