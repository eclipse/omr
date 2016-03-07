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


#include "RegionPoolGeneric.hpp"

bool 
MM_RegionPoolGeneric::initialize(MM_EnvironmentBase *env)
{
	return MM_RegionPool::initialize(env);
}

void 
MM_RegionPoolGeneric::tearDown(MM_EnvironmentBase *env)
{
	MM_RegionPool::tearDown(env);
}

