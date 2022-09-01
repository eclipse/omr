/*******************************************************************************
 * Copyright (c) 2019, 2021 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef SCAVENGERDELEGATE_HPP_
#define SCAVENGERDELEGATE_HPP_

#include "omrcfg.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "modronbase.h"
#include "objectdescription.h"

#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "SlotObject.hpp"

class GC_ObjectScanner;
class MM_CompactScheme;
class MM_EnvironmentStandard;
class MM_ForwardedHeader;
class MM_MarkMap;
class MM_MemorySubSpaceSemiSpace;

class MM_ScavengerDelegate : public MM_BaseVirtual {
private:
	MM_GCExtensionsBase* _extensions;

protected:
public:

private:
protected:
public:

	void kill(MM_EnvironmentBase *env);

	/* Read Barrier Verifier specific methods */
#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
	void poisonSlots(MM_EnvironmentBase *env) {}
	void healSlots(MM_EnvironmentBase *env) {}
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

	/**
	 * This method will be called on the main GC thread after each scavenger cycle, successful or
	 * otherwise. It may be used for reporting or other tasks as required.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] scavengeSuccessful Indicates whether the scavenge cycle completed normally or aborted.
	 */
	void reportScavengeEnd(MM_EnvironmentBase * env, bool scavengeSuccessful);

	/**
	 * This method is called on the main GC thread when a scavenge cycle is started. Implementations of
	 * this method may clear any client-side stats or metadata relating to scavenge cycles at this point.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	void mainSetupForGC(MM_EnvironmentBase * env);

	/**
	 * This method is called on each GC worker thread when a scavenge cycle is started. Implementations of
	 * this method may clear any thread-specific client-side stats or metadata relating to scavenge cycles
	 * at this point.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	void workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase * env);

	/**
	 * This method is called on each GC worker thread when it completes its participation in a scavenge
	 * cycle. It may be used to merge stats collected on each worker thread into global stats.
	 */
	void mergeGCStats_mergeLangStats(MM_EnvironmentBase * env);

	/**
	 * This method is called on the main GC thread when a scavenge cycle completes, successfully or otherwise.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	void mainThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase * env);

	/**
	 * This method is called on the main GC thread when a scavenge cycle completes successfully.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	void mainThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *env);

	/**
	 * If a scavenge cycle is unable to start or complete successfully, the GC may be "percolated" up
	 * to a global GC cycle. This method is called before the decision to percolate is made. If this method
	 * returns true, percolation will be attempted. In that case, the implementation must specify and
	 * return a PercolateReason and a gc code.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[out] percolateReason Reason for percolation
	 * @param[out] gcCode GC code to use for global collection
	 * @return true if GC should percolate to global collection
	 */
	bool internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase * env, PercolateReason * percolateReason, uint32_t * gcCode);

	/**
	 * An object scanner is required to locate and scan all object references associated with each root object
	 * and every object that is evacuated from allocate space during a scavenge cycle. this method must instantiate
	 * in the allocSpace provided an object scanner instance that is appropriate for the object presented. The
	 * allocSpace is guaranteed to be large enough to hold an instance of the largest GC_ObjectScanner subclass
	 * represented in the GC_ObjectScannerState structure.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] objectPtr The object to be scanned
	 * @param[in] allocSpace Space for in-place instantiation of scanner
	 * @param[in] flags See GC_ObjectScanner::InstanceFlags. One of scanRoots or scanHeap will be set , but not both.
	 * @return Pointer to object scanner, or NULL if object not to be scanned (eg, leaf object).
	 * @see GC_ObjectScanner
	 */
	GC_ObjectScanner *getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags);

	/**
	 * Scavenger calls this method when required to force GC threads to flush any locally-held references into
	 * associated global buffers.
	 * 
	 * @param[in] env The environment for the calling thread.
	 */
	void flushReferenceObjects(MM_EnvironmentStandard *env);
	
	/**
	 * An indirect referent is a refernce to an object in the heap that is not a root object and is not reachable from
	 * any root object, for example, a Java Class object. This method is called to determine whether and object is associated
	 * with any indirect objects that are currently in new space.
	 * 
	 * @param[in] env The environment for the calling thread.
	 * @param[in] objectPtr Reference to the object that may have associated indirect referents.
	 * @return true if the object has associated indirect referents.
	 */
	bool hasIndirectReferentsInNewSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);

	/**
	 * This method will be called for any object that has indirect object references. The implementation must call
	 * MM_Scavenger::copyObjectSlot(env, slot) for each reference slot in each indirect object associated with the
	 * object. The object will be included in the remembered set if the method returns true.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] objectPtr Reference to the object that has associated indirect referents.
	 * @return true if MM_Scavenger::copyObjectSlot(env, slot) returns true for any slot.
	 */
	bool scavengeIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);

	/**
	 * If scavenger is unable to complete a cycle eg, because there is insufficient tenure space remaining to promote
	 * an object from new space to tenure space), it must back out of the scavenge cycle and percolate the collection.
	 * In that context, this method will be called for each root object and for each object reachable from a root
	 * object. Implementation must identify all indirect objects associated with the root or reachable object and call
	 * MM_Scavenger::backOutFixSlotWithoutCompression(slot) for each reference slot in each indirect object.
	 *
	 * Backout is a single-threaded operation at present.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] objectPtr Reference to an object that may have associated indirect referents.
	 */
	void backOutIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);

	/* This method must be implemented if an object may hold any object references that are live but not reachable
	 * by traversing the reference graph from the root set or remembered set. In that case, this method should locate
	 * all such objects and call MM_Scavenger::backOutObjectScan(..) for each such object that is in the remembered
	 * set. For example,
	 *
	 * if (_extensions->objectModel.isRemembered(indirectObject)) {
	 *    _extensions->scavenger->backOutObjectScan(env, indirectObject);
	 * }
	 *
	 * This method is called only in backout contexts, and only when the remembered set is in an overflow state.
	 *
	 * Backout is a single-threaded operation at present.
	 *
	 * @param[in] env The environment for the calling thread.
	 */
	void backOutIndirectObjects(MM_EnvironmentStandard *env);

	/**
	 * This method is called during backout while undoing an object copy operation. This may involve restoring the scavenger
	 * forwarding slot, which may have been overwritten in the original copy process, and backing out changes to object
	 * flags in the forwarding slot. With the exception of the scavenger forwarding slot, all of the other bits of the
	 * original copied object should be intact.
	 *
	 * At minimum, if compressed pointers are enabled, this method must call MM_ForwardedHeader::restoreDestroyedOverlap()
	 * to restore the (4-byte) slot overlapping the (8-byte) scavenger forwarding slot.
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] forwardedObject A forwarded header containing the original and destination addresses of the object.
	 */
	void reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject);

#if defined (OMR_GC_COMPRESSED_POINTERS)
	/**
	 * This method is similar to reverseForwardedObject() but is called from a different context. The implementation
	 * must first determine whether or not the compressed slot adjacent to the scavenger forwarding slot may contain an object
	 * reference. In that case, this implementation must:
	 *
	 * 1- get the preserved adjacent slot value from the forwarded header (originalForwardedHeader->getPreservedOverlap())
	 * 2- expand the preserved adjacent slot value to obtain an uncompressed pointer P to the referenced object (see GC_SlotObject)
	 * 3- test whether the referenced object has object alignment and is in new space or tenure space (stop otherwise)
	 * 4- instantiate a new MM_ForwardedHeader instance F using the referenced object pointer
	 * 5- test whether F is a reverse forwarded object (F.isReverseForwardedPointer(), stop otherwise)
	 * 6- call originalForwardedHeader->restoreDestroyedOverlap(T), where T is the compressed pointer for P (see GC_SlotObject)
	 *
	 * @param[in] env The environment for the calling thread.
	 * @param[in] forwardedObject A forwarded header containing the original and destination addresses of the object.
	 * @param[in] subSpaceNew Pointer to new space
	 */
	void fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject, MM_MemorySubSpaceSemiSpace *subSpaceNew);
#endif /* OMR_GC_COMPRESSED_POINTERS */

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/**
	 * Enable/disable language specific thread local resource on Concurrent Scavenger cycle start/end 
	 * @param[in] env The environment for the calling thread.
	 */	 
	void switchConcurrentForThread(MM_EnvironmentBase *env);
	/**
	 * After aborted Concurrent Scavenger, handle indirect object references (off heap structures associated with object with refs to Nursery). 
	 * Fixup should update slots to point to the forwarded version of the object and/or remove self forwarded bit in the object itself.
	 */
	void fixupIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

	bool initialize(MM_EnvironmentBase* env) { return true; }

	void tearDown(MM_EnvironmentBase* env) {}

	MM_ScavengerDelegate(MM_EnvironmentBase* env)
		: MM_BaseVirtual()
		, _extensions(env->getExtensions())
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

#endif /* SCAVENGERDELEGATE_HPP_ */
