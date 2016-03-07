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

#if !defined(REGIONPOOL_HPP_)
#define REGIONPOOL_HPP_

#include "omr.h"

#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"

/**
 * Region pools are abstract classes which manager the group of regions owned by a
 * memory subspace.
 * 
 */

class MM_RegionPool : public MM_BaseVirtual
{
private:
protected:
public:	
	
private:	
protected:
public:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	void kill(MM_EnvironmentBase *env);

	/**
	 * Create a RegionPool object.
	 */
	MM_RegionPool(MM_EnvironmentBase *env) :
		MM_BaseVirtual()
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* REGIONPOOL_HPP_ */
