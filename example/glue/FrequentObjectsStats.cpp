/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2010, 2016
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
#include "FrequentObjectsStats.hpp"
#include "GCExtensionsBase.hpp"
#include "EnvironmentBase.hpp"
#include "ModronAssertions.h"

/**
 * Create and return a new instance of MM_FrequentObjectsStats.
 *
 * @return the new instance, or NULL on failure.
 */
MM_FrequentObjectsStats *
MM_FrequentObjectsStats::newInstance(MM_EnvironmentBase *env)
{
	/* DO NOTHING */
	return NULL;
}


bool
MM_FrequentObjectsStats::initialize(MM_EnvironmentBase *env)
{
	/* DO NOTHING */
	return false;
}

void
MM_FrequentObjectsStats::tearDown(MM_EnvironmentBase *env)
{
	/* DO NOTHING */
	return;
}


void
MM_FrequentObjectsStats::kill(MM_EnvironmentBase *env)
{
	/* DO NOTHING */
	return;
}

void
MM_FrequentObjectsStats::traceStats(MM_EnvironmentBase *env)
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

