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
 * @ingroup GC_Base
 */

#if !defined(SWEEPPOOLMANAGERADDRESSORDEREDLIST_HPP_)
#define SWEEPPOOLMANAGERADDRESSORDEREDLIST_HPP_

#include "omrcfg.h"
#include "modronopt.h"
#include "modronbase.h"

#include "Base.hpp"
#include "EnvironmentBase.hpp"
#include "MemoryPool.hpp"
#include "SweepPoolManagerAddressOrderedListBase.hpp"

class MM_SweepPoolManagerAddressOrderedList : public MM_SweepPoolManagerAddressOrderedListBase
{
private:

protected:

public:

	static MM_SweepPoolManagerAddressOrderedList *newInstance(MM_EnvironmentBase *env);

	/**
	 * Create a SweepPoolManager object.
	 */
	MM_SweepPoolManagerAddressOrderedList(MM_EnvironmentBase *env)
		: MM_SweepPoolManagerAddressOrderedListBase(env)
	{
		_typeId = __FUNCTION__;
	}

};
#endif /* SWEEPPOOLMANAGERADDRESSORDEREDLIST_HPP_ */
