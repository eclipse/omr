/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef EVACUATORDELEGATE_HPP_
#define EVACUATORDELEGATE_HPP_

#include "omr.h"
#include "omrcfg.h"
#include "omrExampleVM.hpp"
#include "omrhashtable.h"

#include "EvacuatorBase.hpp"
#include "EnvironmentStandard.hpp"
#include "ForwardedHeader.hpp"
#include "IndexableObjectScanner.hpp"
#include "MixedObjectScanner.hpp"

class MM_Evacuator;
class MM_EvacuatorController;
class MM_Scavenger;

class MM_EvacuatorDelegate
{
/*
 * Data members
 */
public:
	/**
	 * Controller maintains a volatile bitset of delegate flags, evacuators mutate and test
	 * these to control evacuation stages of each gc cycle. Evacuator delegate may define
	 * flags as required to share this information across evacuators during gc cycles.
	 *
	 * @see MM_EvacuatorController::max_evacuator_public_flag (public bit mask is 0xffffffff)
	 */
	enum {
		unused = 1
	};
private:
	MM_EnvironmentStandard *_env;	/* The environment that is bound to the evacuator instance (from cycle start to end) */
	MM_Scavenger *_controller;		/* The evacuator controller */
	MM_Evacuator *_evacuator;		/* The evacuator instance that this delegate is bound to (1-1) */
	bool _isCleared;				/* This is false when gc cycle starts and is set to true after clearing phase completes */

protected:
public:

/*
 * Function members
 */
private:
protected:
public:
	/**
	 * Evacuator calls this when it instantiates the delegate to bind evacuator-delegate. This
	 * binding is 1-1 and invariant and persists for the lifetime of the delegate.
	 *
	 * @param evacuator the MM_Evacuator instance to bind delegate to
	 * @param forge points to system memory allocator
	 * @controller points to the evacuator controller
	 */
	bool
	initialize(MM_Evacuator *evacuator, MM_Forge *forge, MM_EvacuatorController *controller)
	{
		_evacuator = evacuator;
		_controller = (MM_Scavenger *)controller;
		return true;
	}


	/**
	 * This is called when the OMR vm is shut down, to release resources held by the delegate
	 */
	void tearDown() { }

	/**
	 * This is called from the controller before activating any evacuator instances to allow the
	 * delegate to set up evacuator flags in the controller for the evacuation cycle. The return
	 * value indicates which flags the delegate requires to be set at the start of the gc cycle.
	 * 
	 * A static implementation is necessary because this method is called on the master gc thread
	 * that is bound to the env parameter. At this point no environment is bound to an evacuator,
	 * and env->getEvacuator() will return NULL or a pointer to an unbound evacuator and should 
	 * not be used in the implementation of this method.
	 *
	 * Delegate flags can also be set/cleared using _controller->setEvacuatorFlag(uintptr_t, bool).
	 *
	 * @param env environment for calling thread
	 * @return preset evacuator flags for the cycle
	 */
	static uintptr_t prepareForEvacuation(MM_EnvironmentBase *env) { return 0; }

	/**
	 * Evacuator calls this before it starts work in an evacuation cycle. Implementations may use
	 * this method to prepare for the start of a generational gc cycle.
	 * 
	 * The binding of environment to evacuator-delegate is established at this point. Delegate may 
	 * access bound environment using _evacuator->getEnvironment. 
	 */
	void cycleStart(); /* _env = evacuator->getEnvironment(); */

	/**
	 * Evacuator calls this after it completes work in an evacuation cycle. Implementationsmay use
	 * this method to finalize their involvement in a generational gc cycle.
	 * 
	 * The binding of environment to evacuator-delegate is dissolved after this method completes.
	 */
	void cycleEnd() { _env = NULL; }

	/**
	 * Evacuator calls this to instantiate an object scanner within space provided by objectScannerState, passing
	 * a pointer to its bound environment. This is necessary because MM_Evacuator is an incomplete type and this
	 * method should be inlined for performance.
	 *
	 * @param objectptr the object to be scanned
	 * @param objectScannerState points to space to instantiate the object scanner into
	 * @param flags to be set in the object scanner
	 * @return a pointer to the object scanner
	 */
	GC_ObjectScanner *
	getObjectScanner(omrobjectptr_t objectptr, void *objectScannerState, uintptr_t flags)
	{
		Debug_MM_true(GC_ObjectScanner::isHeapScan(flags) ^ GC_ObjectScanner::isRootScan(flags));
		return GC_MixedObjectScanner::newInstance(_env, objectptr, objectScannerState, flags);
	}

	/**
	 * Evacuator calls this to instantiate an object scanner for a splitable array object within space provided by
	 * objectScannerState. Splitable arrays are indexable objects with contiguous representation in the heap, which
	 * may be split into multiple segments for parallel scanning. The example framework does not support indexable
	 * objects, so this implementation does not split objects for parallel scanning.
	 *
	 * It is assumed that the array elements are contiguous with the object header. For the last segment the end
	 * slot will point just past the end of the image of the object in evacuation space, which may or may not
	 * include additional material, eg hash code, contiguous with the last array element. In that case the
	 * implementation must adjust the end pointer to point just past the end of the last array element.
	 *
	 * @param objectptr the object to be scanned
	 * @param objectScannerState points to space to instantiate the object scanner into
	 * @param splitIndex index (0-based) of first array element to scan in split array segment
	 * @param splitAmount number of array elements to scan in split array segment
	 * @param flags to be set in the object scanner
	 * @return a pointer to the object scanner
	 */
	GC_IndexableObjectScanner *
	getSplitPointerArrayObjectScanner(omrobjectptr_t objectptr, void *objectScannerState, uintptr_t splitIndex, uintptr_t splitAmount, uintptr_t flags)
	{
		Debug_MM_true(false);
		return NULL;
	}

	/**
	 * Return true if teh object is an indexable array of pointers to objects
	 */
	bool isIndexablePointerArray(MM_ForwardedHeader *forwardedHeader) { return false; }

	/**
	 * Locate the base of the contiguous range of indexed object reference slots contained in an indexable object and
	 * count the reference slots contained in the range.
	 *
	 * If the caller's runtime does not support indexable objects this method should return NULL without touching
	 * the caller's element count.
	 */
	fomrobject_t *getIndexableDataBounds(omrobjectptr_t indexableObject, uintptr_t *numberOfElements) { return NULL; }

	/**
	 * Test whether an object has relationships with objects that are not directly accessible by following the object
	 * graph (for example, class statics) but must be evacuated.
	 */
	bool objectHasIndirectObjectsInNursery(omrobjectptr_t objectptr) { return false; }

	/**
	 * Iterate over indirect objects bound to an object, calling _evacuator->evacuateRootObject() for each indirect
	 * object that requires evacuation. If any of these objects are evacuated to nursery space, this method must
	 * return true.
	 */
	bool scanIndirectObjects(omrobjectptr_t objectptr) { return false; }

	/**
	 * Iterate over root objects (eg, heap references held in thread slots when a gc cycle starts), calling
	 * _evacuator->evacuateRootObject() for each root object. An optimzation (obviate a write barrier) may be
	 * realized by calling _evacuator->evacuateThreadSlot() for references from the mutator stack, but this is
	 * not necessary.
	 */
	void scanRoots();

	/**
	 * Reiterate over thread slots, calling _evacuator->rescanThreadSlot() for each thread slot. This is required
	 * only for delegates that call _evacuator->evacuateThreadSlot() for stack-based heap references.
	 */
	void rescanThreadSlots() { }

	/**
	 * Some delegates may elect to hold back some root references until after the main root scanning and heap scan
	 * (and thread rescanning, if used) are complete. This method is called after this point to determine whether
	 * there are more roots to be cleared. If a value true is returned, scanClearable() will be called to allow the
	 * next set of root objects to be cleared.
	 */
	bool hasClearable() { return !_isCleared; }

	/**
	 * If the return value from hasClearable() indicates that there are more roots to be cleared, this method will
	 * be called. As for scanRoots(), _evacuator->evacuateRootObjerct() must be called each root object that is to
	 * be cleared in the present clearing phase.
	 *
	 * Clearing may be partitioned into multiple phases. Each phase begins with a call to scanClearable() after
	 * hasClearable() returns true. All objects reachable in the reference graph from the cleared objects are then
	 * evacuated as required. This repeats until hasClearable() returns false.
	 *
	 */
	void scanClearable();

	/**
	 * This is called before a gc thread enters a wait state (wits for work), to give the delgate an opportunity to
	 * flush any material that it might be holding that may be relevant to other threads. Implementation is entirely
	 * language-specific and may not be required.
	 */
	void flushForWaitState() { }

	/**
	 * This is called after a gc thread completes the last clearing phase, to give the delgate an opportunity to
	 * flush any material that it might be holding that may be relevant to other threads. Implementation is entirely
	 * language-specific and may not be required.
	 */
	void flushForEndCycle() { }

	MM_EvacuatorDelegate()
		: _env(NULL)
		, _controller(NULL)
		, _evacuator(NULL)
		, _isCleared(false)
	{ }

#if defined(EVACUATOR_DEBUG_DELEGATE)
	void debugValidateObject(omrobjectptr_t objectptr) { }
	void debugValidateObject(MM_ForwardedHeader *forwardedHeader) { }
	const char *
	debugGetClassname(omrobjectptr_t objectptr, char *buffer, uintptr_t bufferLength)
	{
		buffer[0] = 0;
		return buffer;
	}
#endif /* defined(EVACUATOR_DEBUG_DELEGATE) */
};
#endif /* EVACUATORDELEGATE_HPP_ */
