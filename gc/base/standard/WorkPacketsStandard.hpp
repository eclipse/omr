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

#if !defined(WORKPACKETSSTANDARD_HPP_)
#define WORKPACKETSSTANDARD_HPP_


#include "EnvironmentStandard.hpp"
#include "WorkPackets.hpp"

class MM_CardTable;

/**
 * @todo Provide class documentation
 */
class MM_WorkPacketsStandard : public MM_WorkPackets
{
/*
 * Data members
 */
private:

protected:

public:
	
/*
 * Function members
 */
private:
	
protected:
	virtual MM_WorkPacketOverflow *createOverflowHandler(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);

public:
	static MM_WorkPacketsStandard  *newInstance(MM_EnvironmentBase *env);

	/**
	 * Create a WorkPackets object.
	 */
	MM_WorkPacketsStandard(MM_EnvironmentBase *env) :
		MM_WorkPackets(env)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* WORKPACKETSSTANDARD_HPP_ */

