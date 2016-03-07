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
#include "omrmodroncore.h"
#include "modronopt.h"

#if defined(OMR_GC_MODRON_COMPACTION)

#include "CompactScheme.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalGCStats.hpp"

#include "ParallelCompactTask.hpp"

uintptr_t
MM_ParallelCompactTask::getVMStateID()
{
	return J9VMSTATE_GC_COMPACT;
}

void
MM_ParallelCompactTask::run(MM_EnvironmentBase *env)
{
	_compactScheme->compact(env, _rebuildMarkBits, _aggressive);
}

void
MM_ParallelCompactTask::setup(MM_EnvironmentBase *env)
{
	env->_compactStats.clear();
}

void
MM_ParallelCompactTask::cleanup(MM_EnvironmentBase *env)
{
	MM_GlobalGCStats *finalGCStats;

	finalGCStats = &MM_GCExtensionsBase::getExtensions(env->getOmrVM())->globalGCStats;
	finalGCStats->compactStats.merge(&env->_compactStats);
}

#endif /* OMR_GC_MODRON_COMPACTION */


