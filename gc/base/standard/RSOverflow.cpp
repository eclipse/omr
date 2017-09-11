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

#include "RSOverflow.hpp"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "MarkMap.hpp"
#include "MarkingScheme.hpp"
#include "HeapMapIterator.hpp"
#include "ObjectModel.hpp"
#include "ParallelGlobalGC.hpp"

void
MM_RSOverflow::initialize(MM_EnvironmentBase *env)
{
	MM_Collector *globalCollector = _extensions->getGlobalCollector();
	Assert_MM_true(NULL != globalCollector);

	/*
	 * Abort Global Collector if necessary to steal Mark Map from it
	 */
	globalCollector->abortCollection(env, ABORT_COLLECTION_SCAVENGE_REMEMBEREDSET_OVERFLOW);

	/* to get Mark Map need Marking Scheme first */
	MM_MarkingScheme *markingScheme = ((MM_ParallelGlobalGC *)globalCollector)->getMarkingScheme();
	Assert_MM_true(NULL != markingScheme);

	/* get Mark Map */
	_markMap = markingScheme->getMarkMap();
	Assert_MM_true(NULL != _markMap);

	/* Clean stolen Mark Map */
	_markMap->initializeMarkMap(env);
}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
