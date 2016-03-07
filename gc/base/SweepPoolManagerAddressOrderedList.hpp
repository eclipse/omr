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
