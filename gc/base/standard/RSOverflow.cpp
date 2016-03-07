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
