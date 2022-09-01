/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "omrcfg.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "ScavengerDelegate.hpp"

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

void
MM_ScavengerDelegate::mainSetupForGC(MM_EnvironmentBase *env)
{
	/* Do nothing for now */
}

void
MM_ScavengerDelegate::workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase *env)
{
	/* Do nothing for now */
}

void
MM_ScavengerDelegate::reportScavengeEnd(MM_EnvironmentBase * envBase, bool scavengeSuccessful)
{
	/* Do nothing for now */
}

void
MM_ScavengerDelegate::mergeGCStats_mergeLangStats(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

void
MM_ScavengerDelegate::mainThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

void
MM_ScavengerDelegate::mainThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

bool
MM_ScavengerDelegate::internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase *envBase, PercolateReason *reason, uint32_t *gcCode)
{
	/* Do nothing for now */
	return false;
}

GC_ObjectScanner *
MM_ScavengerDelegate::getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags)
{
#if defined(OMR_GC_MODRON_SCAVENGER_STRICT)
	Assert_MM_true((GC_ObjectScanner::scanHeap == flags) ^ (GC_ObjectScanner::scanRoots == flags));
#endif /* defined(OMR_GC_MODRON_SCAVENGER_STRICT) */
	GC_ObjectScanner *objectScanner = NULL;
	objectScanner = GC_MixedObjectScanner::newInstance(env, objectPtr, allocSpace, flags);
	return objectScanner;
}

void
MM_ScavengerDelegate::flushReferenceObjects(MM_EnvironmentStandard *env)
{
	/* Do nothing for now */
}

bool
MM_ScavengerDelegate::hasIndirectReferentsInNewSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* This method must be implemented and return true an object may hold any object references that are live
	 * but not reachable by traversing the reference graph from the root set or remembered set. Otherwise this
	 * default implementation should be used.
	 */
	return false;
}

bool
MM_ScavengerDelegate::scavengeIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* This method must be implemented if an object may hold any object references that are live but not reachable
	 * by traversing the reference graph from the root set or remembered set. In that case, this method should
	 * call MM_Scavenger::copyObjectSlot(..) for each such indirect object reference, ORing the boolean result
	 * from each call to MM_Scavenger::copyObjectSlot(..) into a single boolean value to be returned.
	 */
	return false;
}

void
MM_ScavengerDelegate::backOutIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* This method must be implemented if an object may hold any object references that are live but not reachable
	 * by traversing the reference graph from the root set or remembered set. In that case, this method should
	 * call MM_Scavenger::backOutFixSlotWithoutCompression(..) for each uncompressed slot holding a reference to
	 * an indirect object that is associated with the object.
	 */
}

void
MM_ScavengerDelegate::backOutIndirectObjects(MM_EnvironmentStandard *env)
{
	/* This method must be implemented if an object may hold any object references that are live but not reachable
	 * by traversing the reference graph from the root set or remembered set. In that case, this method should locate
	 * all such objects and call MM_Scavenger::backOutObjectScan(..) for each such object that is in the remembered
	 * set. For example,
	 *
	 * if (_extensions->objectModel.isRemembered(indirectObject)) {
	 *    _extensions->scavenger->backOutObjectScan(env, indirectObject);
	 * }
	 */
}

void
MM_ScavengerDelegate::reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader)
{
	if (forwardedHeader->isForwardedPointer()) {
		omrobjectptr_t originalObject = forwardedHeader->getObject();
		omrobjectptr_t forwardedObject = forwardedHeader->getForwardedObject();

		/* Restore the original object header from the forwarded object */
		GC_ObjectModel *objectModel = &(env->getExtensions()->objectModel);

		originalObject->header.assign(
			(ObjectSize)objectModel->getConsumedSizeInBytesWithHeader(forwardedObject),
			(uint8_t)objectModel->getObjectFlags(forwardedObject));

#if defined (OMR_GC_COMPRESSED_POINTERS)
		if (env->compressObjectReferences()) {
			/* Restore destroyed overlapped slot in the original object. This slot might need to be reversed
			 * as well or it may be already reversed - such fixup will be completed at in a later pass.
			 */
			forwardedHeader->restoreDestroyedOverlap();
		}
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */
	}
}

#if defined (OMR_GC_COMPRESSED_POINTERS)
void
MM_ScavengerDelegate::fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader, MM_MemorySubSpaceSemiSpace *subSpaceNew)
{
	bool const compressed = _extensions->compressObjectReferences();
	/* This method must be implemented if (and only if) the object header is stored in a compressed slot. in that
	 * case the other half of the full (omrobjectptr_t sized) slot may hold a compressed object reference that
	 * must be restored by this method.
	 */
	/* This assumes that all slots are object slots, including the slot adjacent to the header slot */
	if ((0 != forwardedHeader->getPreservedOverlap()) && !_extensions->objectModel.isIndexable(forwardedHeader)) {
		OMR_VM* omrVM = _extensions->getOmrVM();
		/* Get the uncompressed reference from the slot */
		fomrobject_t preservedOverlap = (fomrobject_t)forwardedHeader->getPreservedOverlap();
		GC_SlotObject preservedSlotObject(omrVM, &preservedOverlap);
		omrobjectptr_t survivingCopyAddress = preservedSlotObject.readReferenceFromSlot();
		/* Check if the address we want to read is aligned (since mis-aligned reads may still be less than a top address but extend beyond it) */
		if (0 == ((uintptr_t)survivingCopyAddress & (_extensions->getObjectAlignmentInBytes() - 1))) {
			/* Ensure that the address we want to read is within part of the heap which could contain copied objects (tenure or survivor) */
			void *topOfObject = (void *)((uintptr_t *)survivingCopyAddress + 1);
			if (subSpaceNew->isObjectInNewSpace(survivingCopyAddress, topOfObject) || _extensions->isOld(survivingCopyAddress, topOfObject)) {
				/* if the slot points to a reverse-forwarded object, restore the original location (in evacuate space) */
				MM_ForwardedHeader reverseForwardedHeader(survivingCopyAddress, compressed);
				if (reverseForwardedHeader.isReverseForwardedPointer()) {
					/* overlapped slot must be fixed up */
					fomrobject_t fixupSlot = 0;
					GC_SlotObject fixupSlotObject(omrVM, &fixupSlot);
					fixupSlotObject.writeReferenceToSlot(reverseForwardedHeader.getReverseForwardedPointer());
					forwardedHeader->restoreDestroyedOverlap((uint32_t)fixupSlot);
				}
			}
		}
	}
}
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
