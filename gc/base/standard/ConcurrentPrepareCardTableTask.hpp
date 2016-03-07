/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(CONCURRENTPREPARECARDTABLETASK_HPP_)
#define CONCURRENTPREPARECARDTABLETASK_HPP_

#include "omrcfg.h"
#include "omrmodroncore.h"

#include "ParallelTask.hpp"
#include "ConcurrentCardTableForWC.hpp"

class MM_Dispatcher;
class MM_EnvironmentBase;

/**
 * Task used to prepare the card-table in parallel on weakly-ordered platforms.
 * @ingroup GC_Modron_Standard
 */
class MM_ConcurrentPrepareCardTableTask : public MM_ParallelTask
{
private:
	MM_ConcurrentCardTableForWC *_cardTable;
	Card *_firstCard;
	Card *_lastCard;
	CardAction _action;

public:
	virtual UDATA getVMStateID() { return J9VMSTATE_GC_CONCURRENT_MARK_PREPARE_CARD_TABLE; }
	
	virtual void run(MM_EnvironmentBase *env);

	MM_ConcurrentPrepareCardTableTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_ConcurrentCardTableForWC *cardTable, Card *firstCard, Card *lastCard, CardAction action) :
		MM_ParallelTask(env, dispatcher),
		_cardTable(cardTable),
		_firstCard(firstCard),
		_lastCard(lastCard),
		_action(action)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* CONCURRENTPREPARECARDTABLETASK_HPP_ */
