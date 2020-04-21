/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "j9nongenerated.h"
#include "modronbase.h"

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "CardTable.hpp"
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#include "CollectorLanguageInterfaceImpl.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "ConcurrentSafepointCallback.hpp"
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#if defined(OMR_GC_MODRON_COMPACTION)
#include "CompactScheme.hpp"
#endif /* OMR_GC_MODRON_COMPACTION */
#include "EnvironmentStandard.hpp"
#include "ForwardedHeader.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "MarkingScheme.hpp"
#include "MemorySubSpaceSemiSpace.hpp"
#include "MixedObjectScanner.hpp"
#include "mminitcore.h"
#include "objectdescription.h"
#include "ObjectIterator.hpp"
#include "ObjectModel.hpp"
#include "omr.h"
#include "omrExampleVM.hpp"
#include "omrvm.h"
#include "OMRVMInterface.hpp"
#include "ParallelGlobalGC.hpp"
#include "Scavenger.hpp"
#include "SlotObject.hpp"

/**
 * Initialization
 */
MM_CollectorLanguageInterfaceImpl *
MM_CollectorLanguageInterfaceImpl::newInstance(MM_EnvironmentBase *env)
{
	MM_CollectorLanguageInterfaceImpl *cli = NULL;
	OMR_VM *omrVM = env->getOmrVM();
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);

	cli = (MM_CollectorLanguageInterfaceImpl *)extensions->getForge()->allocate(sizeof(MM_CollectorLanguageInterfaceImpl), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != cli) {
		new(cli) MM_CollectorLanguageInterfaceImpl(omrVM);
		if (!cli->initialize(omrVM)) {
			cli->kill(env);
			cli = NULL;
		}
	}

	return cli;
}

void
MM_CollectorLanguageInterfaceImpl::kill(MM_EnvironmentBase *env)
{
	OMR_VM *omrVM = env->getOmrVM();
	tearDown(omrVM);
	MM_GCExtensionsBase::getExtensions(omrVM)->getForge()->free(this);
}

void
MM_CollectorLanguageInterfaceImpl::tearDown(OMR_VM *omrVM)
{

}

bool
MM_CollectorLanguageInterfaceImpl::initialize(OMR_VM *omrVM)
{
	return true;
}
