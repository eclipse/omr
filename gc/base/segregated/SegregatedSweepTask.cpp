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

#include "EnvironmentBase.hpp"
#include "SweepSchemeSegregated.hpp"

#include "SegregatedSweepTask.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

void
MM_SegregatedSweepTask::run(MM_EnvironmentBase *env)
{
	_sweepScheme->sweep(env, _memoryPool, false);
}

void
MM_SegregatedSweepTask::setup(MM_EnvironmentBase *env)
{

}

void
MM_SegregatedSweepTask::cleanup(MM_EnvironmentBase *envBase)
{

}

#endif /* OMR_GC_SEGREGATED_HEAP */

