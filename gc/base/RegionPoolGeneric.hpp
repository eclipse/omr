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
