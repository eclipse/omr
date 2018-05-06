/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017
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

#include "ConcurrentMarkingDelegate.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "ConcurrentGC.hpp"
#include "MarkingScheme.hpp"

bool
MM_ConcurrentMarkingDelegate::initialize(MM_EnvironmentBase* env, MM_ConcurrentGC* collector)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	_objectModel                    = &(extensions->objectModel);
	_markingScheme                  = collector->getMarkingScheme();
	_collector                      = collector;
	return true;
}

uintptr_t
MM_ConcurrentMarkingDelegate::collectRoots(
	MM_EnvironmentBase* env, uintptr_t concurrentStatus, bool* collectedRoots, bool* paidTax)
{
	uintptr_t bytesScanned = 0;
	*collectedRoots        = true;
	*paidTax               = true;

	switch (concurrentStatus) {
	case CONCURRENT_ROOT_TRACING1: _markingScheme->markLiveObjectsRoots(env); break;
	default: Assert_MM_unreachable();
	}

	return bytesScanned;
}
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
