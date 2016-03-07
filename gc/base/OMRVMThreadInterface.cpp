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


#include "omrcfg.h"

#include "CollectorLanguageInterface.hpp"
#include "EnvironmentBase.hpp"
#include "ObjectAllocationInterface.hpp"

#include "OMRVMThreadInterface.hpp"

void
GC_OMRVMThreadInterface::flushCachesForWalk(MM_EnvironmentBase *env)
{
	env->_objectAllocationInterface->flushCache(env);
}

void
GC_OMRVMThreadInterface::flushNonAllocationCaches(MM_EnvironmentBase *env)
{
	env->getExtensions()->collectorLanguageInterface->flushNonAllocationCaches(env);
}

void
GC_OMRVMThreadInterface::flushCachesForGC(MM_EnvironmentBase *env)
{
	flushCachesForWalk(env);
	flushNonAllocationCaches(env);
}
