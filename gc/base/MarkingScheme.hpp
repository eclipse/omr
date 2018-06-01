/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#if !defined(MARKINGSCHEME_HPP_)
#define MARKINGSCHEME_HPP_

#include "omrcfg.h"
#include "omr.h"
#include "omrgcconsts.h"

#include "BaseVirtual.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "MarkingDelegate.hpp"
#include "MarkMap.hpp"
#include "ModronAssertions.h"
#include "ObjectModel.hpp"
#include "ObjectScannerState.hpp"
#include "WorkStack.hpp"

#include "ObjectSlotWalker.hpp"

/**
 * @todo Provide class documentation
 */
class MM_MarkingScheme : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:
	OMR_VM *_omrVM;

protected:
	MM_GCExtensionsBase *_extensions;
	MM_MarkingDelegate _delegate;
	MM_MarkMap *_markMap;
	MM_WorkPackets *_workPackets;
	void *_heapBase;
	void *_heapTop;

public:

	/*
	 * Function members
	 */
private:
	MMINLINE void
	assertSaneObjectPtr(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
	{
		Assert_GC_true_with_message(env, objectPtr != J9_INVALID_OBJECT, "Invalid object pointer %p\n", objectPtr);
		Assert_MM_objectAligned(env, objectPtr);
		Assert_GC_true_with_message3(env, isHeapObject(objectPtr), "Object %p not in heap range [%p,%p)\n", objectPtr, _heapBase, _heapTop);
		
		assertNotForwardedPointer(env, objectPtr);
	}
	
	void assertNotForwardedPointer(MM_EnvironmentBase *env, omrobjectptr_t objectPtr);

	/**
	 * Private internal. Called exclusively from completeScan();
	 */
	MMINLINE uintptr_t scanObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr);

	MM_WorkPackets *createWorkPackets(MM_EnvironmentBase *env);

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

public:

	struct MarkingVisitor {
		MarkingVisitor(MM_EnvironmentBase* env, MM_MarkingScheme* markingScheme) :
			env(env), markingScheme(markingScheme) {}
		
		MM_EnvironmentBase* env;
		MM_MarkingScheme* markingScheme;

		template <class SlotHandleT>
		void edge(void* referee, SlotHandleT slot) {
			markingScheme->inlineMarkObjectNoCheck(env, slot.readReference());
		}
	};

	static MM_MarkingScheme *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	
	void masterSetupForGC(MM_EnvironmentBase *env);
	void masterSetupForWalk(MM_EnvironmentBase *env);
	void masterCleanupAfterGC(MM_EnvironmentBase *env);
	void workerSetupForGC(MM_EnvironmentBase *env);
	void workerCleanupAfterGC(MM_EnvironmentBase *env);
	void completeMarking(MM_EnvironmentBase *env);

	/**
	 *  Initialization for Mark
	 *  Actual startup for Mark procedure
	 *  @param[in] env - passed Environment 
	 *  @param[in] initMarkMap instruct should mark map be initialized too
	 */
	void markLiveObjectsInit(MM_EnvironmentBase *env, bool initMarkMap);

	/**
	 *  Mark Roots
	 *  Create Root Scanner and Mark all roots including classes and classloaders if dynamic class unloading is enabled
	 *  @param[in] env - passed Environment 
	 */
	void markLiveObjectsRoots(MM_EnvironmentBase *env);

	/**
	 *  Scan (complete)
	 *  Process all work packets from queue, including classes and work generated during this phase.
	 *  On return, all live objects will be marked, although some unreachable objects (such as finalizable 
	 *  objects) may still be revived by by subsequent stages.   
	 *  @param[in] env - passed Environment 
	 */
	void markLiveObjectsScan(MM_EnvironmentBase *env);
	
	/**
	 *  Final Mark services including scanning of Clearables
	 *  @param[in] env - passed Environment 
	 */
	void markLiveObjectsComplete(MM_EnvironmentBase *env);


	/**
	 * Fast marking method requires caller to verify that the object pointer is not NULL.
	 *
	 * @param[in] env calling thread environment
	 * @param[in] objectPtr pointer to object to be marked (must not be NULL)
	 * @param[in] leafType pass true if object is known to be a leaf (holds no object references)
	 * @return true if the object was marked in this call, false if previously marked by other thread
	 * @see inlineMarkObject()
	 */
	MMINLINE bool
	inlineMarkObjectNoCheck(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool leafType = false)
	{
		assertSaneObjectPtr(env, objectPtr);

		/* If bit not already set in mark map then set it */
		if (!_markMap->atomicSetBit(objectPtr)) {
			return false;
		}

		/* mark successful - Attempt to add to the work stack */
		if (!leafType) {
			env->_workStack.push(env, (void *)objectPtr);
		}

		env->_markStats._objectsMarked += 1;

		return true;
	}

	/**
	 * Convenience method performs NULL check for inlineMarkObjectNoCheck()
	 *
	 * @param[in] env calling thread environment
	 * @param[in] objectPtr pointer to object to be marked (must not be NULL)
	 * @param[in] leafType pass true if object is known to be a leaf (holds no object references)
	 * @return true if the object was marked in this call, false if previously marked by other thread
	 * @see inlineMarkObjectNoCheck()
	 */
	MMINLINE bool
	inlineMarkObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool leafType=false)
	{
		bool objectMarked = false;

		if (NULL != objectPtr) {
			objectMarked = inlineMarkObjectNoCheck(env, objectPtr, leafType);
		}

		return objectMarked;
	}

	/**
	 * Out-of-line wrapper for inlineMarkObject()
	 * @see inlineMarkObject()
	 */
	bool markObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool leafType = false);


	/**
	 * Out-of-line wrapper for inlineMarkObjectNoCheck()
	 * @see inlineMarkObjectNoCheck()
	 */
	bool markObjectNoCheck(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool leafType = false);

	/**
	 * Test marked status of object. Everything off-heap is considered to be marked.
	 * @param[in] obejctPtr object to test
	 * @return true if object is marked (or off heap)
	 */
	MMINLINE bool
	isMarked(omrobjectptr_t objectPtr)
	{
		bool marked = true;
		
		/* Everything off-heap is considered marked, everything on-heap must be checked */
		if (isHeapObject(objectPtr)) {
			marked = _markMap->isBitSet(objectPtr);
		}

		return marked;
	}

	uintptr_t numMarkBitsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop);
	uintptr_t setMarkBitsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop, bool clear);
	uintptr_t numHeapBytesPerMarkMapByte() { return (_markMap->getObjectGrain() * BITS_PER_BYTE); };

	/**
	 * This is the marking workhorse, which burns down the backlog of work packets that have accumulated
	 * on the global work stack during marking. It calls scanObject() for every object in the work stack 
	 * until the work stack is empty.
	 */
	void completeScan(MM_EnvironmentBase *env);
	


	/**
	 * Public object scanning method. Called from external context, eg concurrent GC. Scans object slots
	 * and marks objects referenfced from specified object.
	 *
	 * For pointer arrays, which may be split for scanning, caller may specify a maximum number
	 * of bytes to scan. For scalar object type, the default value for this parameter works fine.
	 *
	 * @param[in] env calling thread environment
	 * @param[in] objectPtr pointer to object to be marked (must not be NULL)
	 * @param[in] reason enumerator identifying scanning context
	 * @param sizeToDo maximum number of bytes to scan, for pointer arrays
	 */
	MMINLINE uintptr_t
	scanObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_MarkingSchemeScanReason reason, uintptr_t sizeToDo = UDATA_MAX)
	{
		OMRApp::ObjectSlotWalker walker;
		walker.traverse(objectPtr, MarkingVisitor(env, this));
		return 0;
	}

	MM_MarkingDelegate *getMarkingDelegate() { return &_delegate; }

	MM_MarkMap *getMarkMap() { return _markMap; }
	void setMarkMap(MM_MarkMap *markMap) { _markMap = markMap; }
	
	bool isMarkedOutline(omrobjectptr_t objectPtr);
	MM_WorkPackets *getWorkPackets() { return _workPackets; }
	
	bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);

	MMINLINE bool isHeapObject(omrobjectptr_t objectPtr)
	{
		return ((_heapBase <= (uint8_t *)objectPtr) && (_heapTop > (uint8_t *)objectPtr));
	}

	/**
	 * Update the slot value to point to (Scavenger) forwarded object. If self-forwarded,
	 * the slot is unchanged, but the class slot of the self-forwarded object is restored (self-forwarded bits are reset).
	 * This is currently used only in presence of Concurrent Scavenger. When abort is detected CS does not fix up 
	 * heap references within Scavenger abort handling, but relies on marking in percolate Global to do it.
	 * The updates are non-atomic, but even though multiple threads could be doing it, they set it to the same value.
	 * During Concurrent Marking, ignore forwarded objects (especially true for self-forwarded objects which are very important
	 * during Scavenger aborted cycle to prevent duplicate copies). The fixup will be done after Scavenger Cycle is done, 
	 * in the final phase of Concurrent GC when we scan Nursery. 
	 */

	void fixupForwardedSlot(GC_SlotObject *slotObject) {
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		if (_extensions->isConcurrentScavengerEnabled() && _extensions->isScavengerBackOutFlagRaised()) {
			fixupForwardedSlotOutline(slotObject);
		}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
	}
	
	void fixupForwardedSlotOutline(GC_SlotObject *slotObject);

	/**
	 * Create a MarkingScheme object.
	 */
	MM_MarkingScheme(MM_EnvironmentBase *env)
		: MM_BaseVirtual()
		, _omrVM(env->getOmrVM())
		, _extensions(env->getExtensions())
		, _delegate()
		, _markMap(NULL)
		, _workPackets(NULL)
		, _heapBase(NULL)
		, _heapTop(NULL)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* MARKINGSCHEME_HPP_ */
