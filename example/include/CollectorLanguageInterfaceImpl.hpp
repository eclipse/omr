/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef COLLECTORLANGUAGEINTERFACEIMPL_HPP_
#define COLLECTORLANGUAGEINTERFACEIMPL_HPP_

#include "modronbase.h"
#include "omr.h"

#include "CollectorLanguageInterface.hpp"
#include "GCExtensionsBase.hpp"
#include "ParallelSweepScheme.hpp"
#include "WorkPackets.hpp"

class GC_ObjectScanner;
class MM_CompactScheme;
class MM_EnvironmentStandard;
class MM_ForwardedHeader;
class MM_MarkingScheme;
class MM_MemorySubSpaceSemiSpace;

/**
 * Class representing a collector language interface.  This implements the API between the OMR
 * functionality and the language being implemented.
 */
class MM_CollectorLanguageInterfaceImpl : public MM_CollectorLanguageInterface {
private:
protected:
	OMR_VM *_omrVM;
	MM_GCExtensionsBase *_extensions;
public:

private:
protected:
	bool initialize(OMR_VM *omrVM);
	void tearDown(OMR_VM *omrVM);

	MM_CollectorLanguageInterfaceImpl(OMR_VM *omrVM)
		: MM_CollectorLanguageInterface()
		,_omrVM(omrVM)
		,_extensions(MM_GCExtensionsBase::getExtensions(omrVM))
	{
		_typeId = __FUNCTION__;
	}

public:
	static MM_CollectorLanguageInterfaceImpl *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

#if defined(OMR_GC_MODRON_SCAVENGER)
	virtual void scavenger_masterSetupForGC(MM_EnvironmentBase *env);
	virtual void scavenger_workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase *env);
	virtual void scavenger_reportScavengeEnd(MM_EnvironmentBase * envBase, bool scavengeSuccessful);
	virtual void scavenger_mergeGCStats_mergeLangStats(MM_EnvironmentBase *envBase);
	virtual void scavenger_masterThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase *envBase);
	virtual void scavenger_masterThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *envBase);
	virtual bool scavenger_internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase *envBase, PercolateReason *reason, uint32_t *gcCode);
	virtual GC_ObjectScanner *scavenger_getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags);
	virtual void scavenger_flushReferenceObjects(MM_EnvironmentStandard *env);
	virtual bool scavenger_hasIndirectReferentsInNewSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	virtual bool scavenger_scavengeIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	virtual void scavenger_backOutIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
	virtual void scavenger_backOutIndirectObjects(MM_EnvironmentStandard *env);
	virtual void scavenger_reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader);
#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	virtual void scavenger_fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader, MM_MemorySubSpaceSemiSpace *subSpaceNew);
#endif /* OMR_INTERP_COMPRESSED_OBJECT_HEADER */
#endif /* OMR_GC_MODRON_SCAVENGER */

};

#endif /* COLLECTORLANGUAGEINTERFACEIMPL_HPP_ */
