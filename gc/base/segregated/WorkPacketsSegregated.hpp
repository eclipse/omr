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

#if !defined(WORKPACKETSSEGREGATED_HPP_)
#define WORKPACKETSSEGREGATED_HPP_

#include "WorkPackets.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_EnvironmentBase;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_WorkPacketsSegregated : public MM_WorkPackets
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
	static MM_WorkPacketsSegregated  *newInstance(MM_EnvironmentBase *env);

	/**
	 * Create a WorkPackets object.
	 * @ingroup GC_Modron_Standard methodGroup
	 */
	MM_WorkPacketsSegregated(MM_EnvironmentBase *env) :
		MM_WorkPackets(env)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* WORKPACKETSSEGREGATED_HPP_ */

