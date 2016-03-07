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

#if !defined(WORKPACKETSCONCURRENT_HPP_)
#define WORKPACKETSCONCURRENT_HPP_


#include "EnvironmentStandard.hpp"
#include "WorkPacketsStandard.hpp"

class MM_CardTable;

/**
 * @todo Provide class documentation
 */
class MM_WorkPacketsConcurrent : public MM_WorkPacketsStandard
{
private:

protected:

public:

private:

protected:
	virtual MM_WorkPacketOverflow *createOverflowHandler(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);

public:
	static MM_WorkPacketsConcurrent  *newInstance(MM_EnvironmentBase *env);

	void resetWorkPacketsOverflow();

	/**
	 * Create a WorkPackets object.
	 * @ingroup GC_Modron_Standard methodGroup
	 */
	MM_WorkPacketsConcurrent(MM_EnvironmentBase *env) :
		MM_WorkPacketsStandard(env)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* WORKPACKETSCONCURRENT_HPP_ */

