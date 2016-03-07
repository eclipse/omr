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


#if !defined(OMRVMINTERFACE_HPP_)
#define OMRVMINTERFACE_HPP_

#include "omrcfg.h"
#include "omr.h"

class MM_EnvironmentBase;
class MM_GCExtensionsBase;
struct J9HookInterface;

/**
 * @todo Provide class documentation
 */
class GC_OMRVMInterface
{
private:
protected:
public:
	static void flushCachesForWalk(OMR_VM* omrVM);
	static void flushCachesForGC(MM_EnvironmentBase *env);
	static void flushNonAllocationCaches(MM_EnvironmentBase *env);
	static void initializeExtensions(MM_GCExtensionsBase *extensions);
	static J9HookInterface** getOmrHookInterface(MM_GCExtensionsBase *extensions);
};

#endif /* OMRVMINTERFACE_HPP_ */
