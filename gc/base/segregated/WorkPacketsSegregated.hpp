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

