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
