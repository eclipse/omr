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

