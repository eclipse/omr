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
 * @ingroup GC_Modron_Standard
 */
 
#include "CompactFixHeapForWalkTask.hpp"
#include "CompactScheme.hpp"

#if defined(OMR_GC_MODRON_COMPACTION)

void 
MM_CompactFixHeapForWalkTask::run(MM_EnvironmentBase *env)
{
	_compactScheme->parallelFixHeapForWalk(env);
}
 
#endif /* OMR_GC_MODRON_COMPACTION */

