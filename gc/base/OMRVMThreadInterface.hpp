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


class MM_EnvironmentBase;

#if !defined(OMRVMTHREADINTERFACE_HPP_)
#define OMRVMTHREADINTERFACE_HPP_
#include "omrcfg.h"

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class GC_OMRVMThreadInterface
{
public:
	static void flushCachesForWalk(MM_EnvironmentBase *env);
	static void flushCachesForGC(MM_EnvironmentBase *env);
	static void flushNonAllocationCaches(MM_EnvironmentBase *env);
};

#endif /* OMRVMTHREADINTERFACE_HPP_ */

