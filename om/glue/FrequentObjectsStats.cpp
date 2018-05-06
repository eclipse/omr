/*******************************************************************************
 * Copyright (c) 2010, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "FrequentObjectsStats.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "ModronAssertions.h"

/**
 * Create and return a new instance of MM_FrequentObjectsStats.
 *
 * @return the new instance, or NULL on failure.
 */
MM_FrequentObjectsStats*
MM_FrequentObjectsStats::newInstance(MM_EnvironmentBase* env)
{
	/* DO NOTHING */
	return NULL;
}

bool
MM_FrequentObjectsStats::initialize(MM_EnvironmentBase* env)
{
	/* DO NOTHING */
	return false;
}

void
MM_FrequentObjectsStats::tearDown(MM_EnvironmentBase* env)
{
	/* DO NOTHING */
	return;
}

void
MM_FrequentObjectsStats::kill(MM_EnvironmentBase* env)
{
	/* DO NOTHING */
	return;
}

void
MM_FrequentObjectsStats::traceStats(MM_EnvironmentBase* env)
{
	/* DO NOTHING */
	return;
}

void
MM_FrequentObjectsStats::merge(MM_FrequentObjectsStats* frequentObjectsStats)
{
	/* DO NOTHING */
	return;
}
