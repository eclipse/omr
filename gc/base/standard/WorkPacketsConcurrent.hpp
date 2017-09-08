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

