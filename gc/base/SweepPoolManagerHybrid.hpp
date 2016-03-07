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
 ******************************************************************************/

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(SWEEPPOOLMANAGERHYBRID_HPP_)
#define SWEEPPOOLMANAGERHYBRID_HPP_

#include "omrcfg.h"
#include "modronopt.h"
#include "modronbase.h"

#if defined(OMR_GC_MODRON_STANDARD)
#include "SweepPoolManagerSplitAddressOrderedList.hpp"

class MM_SweepPoolManagerHybrid : public MM_SweepPoolManagerSplitAddressOrderedList
{
private:
protected:
public:

	static MM_SweepPoolManagerHybrid *newInstance(MM_EnvironmentBase *env);

	/**
	 * Create a SweepPoolManager object.
	 */
	MM_SweepPoolManagerHybrid(MM_EnvironmentBase *env)
		: MM_SweepPoolManagerSplitAddressOrderedList(env)
	{
		_typeId = __FUNCTION__;
	}

};

#endif /* defined(OMR_GC_MODRON_STANDARD) */
#endif /* SWEEPPOOLMANAGERHYBRID_HPP_ */
