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
 * @ingroup GC_Base_Core
 */

#if !defined(REGIONPOOLGENERIC_HPP_)
#define REGIONPOOLGENERIC_HPP_

#include "omr.h"

#include "RegionPool.hpp"

/**
 * A simple region pool which manages a single region.
 * 
 * @ingroup GC_Base_Core
 */
class MM_RegionPoolGeneric : public MM_RegionPool
{
private:
protected:
public:	
	
private:	
protected:
public:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	/**
	 * Create a RegionPoolGeneric object.
	 */
	MM_RegionPoolGeneric(MM_EnvironmentBase *env) :
		MM_RegionPool(env)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* REGIONPOOLGENERIC_HPP_ */
