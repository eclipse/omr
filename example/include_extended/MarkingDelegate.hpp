/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#if !defined(MARKINGDELEGATE_HPP_)
#define MARKINGDELEGATE_HPP_

#include "objectdescription.h"
#include "omrgcconsts.h"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "MixedObjectScanner.hpp"
#include "Task.hpp"

class MM_EnvironmentBase;
class MM_MarkingScheme;

/**
 * Provides language-specific support for marking.
 */
class MM_MarkingDelegate
{
	/*
	 * Data members
	 */
private:

protected:
	GC_ObjectModel *_objectModel;
	MM_MarkingScheme *_markingScheme;

public:

	/*
	 * Function members
	 */
private:

protected:

public:
	/**
	 * Initialize the delegate.
	 *
	 * @param env environment for calling thread
	 * @param markingScheme the MM_MarkingScheme that the delegate is bound to
	 * @return true if delegate initialized successfully
	 */
	MMINLINE bool
	initialize(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme)
	{
		_objectModel = &(env->getExtensions()->objectModel);
		_markingScheme = markingScheme;
		return true;
	}

	/**
	 * This method is called on the master garbage collection thread at the beginning of the marking
	 * phase of a global GC cycle. Any language-specific setup that is required to support marking
	 * can be performed here.
	 *
	 * This method is informational, no specific action is required.
	 *
	 * @param env The environment for the calling thread
	 */
	MMINLINE void masterSetupForGC(MM_EnvironmentBase *env) { }

	/**
	 * This method is called on the master garbage collection thread at the beginning of the marking
	 * phase preceding a heap walk. Any language-specific setup that is required to prepare for the
	 * heap walk can be performed here.
	 *
	 * This method is informational, no specific action is required.
	 *
	 * @param env the current environment
	 */
	MMINLINE void masterSetupForWalk(MM_EnvironmentBase *env) { }

	/**
	 * This method is called on the master garbage collection thread at the end of the marking phase
	 * for a global GC cycle. Any language-specific finalization that is required to support marking
	 * can be performed here.
	 *
	 * This method is informational, no specific action is required.
	 *
	 * @param env the current environment
	 */
	void masterCleanupAfterGC(MM_EnvironmentBase *env);

	/**
	 * This method is called on each worker thread participating in garbage collection at the beginning
	 * of the marking phase of a global GC cycle. Any language-specific actions required to support
	 * marking can be performed here.
	 *
	 * This method is informational, no specific action is required.
	 *
	 * @param env The environment for the calling thread
	 */
	MMINLINE void workerSetupForGC(MM_EnvironmentBase *env) { }

	/**
	 * This method is called on each worker thread participating in garbage collection at the end
	 * of the live object marking phase of a global GC cycle. Any language-specific actions required
	 * to support additional marking can be performed here.
	 *
	 * This method is informational, no specific action is required.
	 *
	 * @param env The environment for the calling thread
	 */
	MMINLINE void workerCompleteGC(MM_EnvironmentBase *env)
	{
		/* All threads flush buffers before this point, and complete any remaining language-specific marking tasks */
		if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
			/* Perform single-threaded tasks here */
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
	}

	/**
	 * This method is called on each worker thread participating in garbage collection at the end of a
	 * global GC cycle. Any final language-specific actions related to marking can be performed here.
	 *
	 * This method is informational, no specific action is required.
	 *
	 * @param env The environment for the calling thread
	 */
	MMINLINE void workerCleanupAfterGC(MM_EnvironmentBase *env) { }

	/**
	 * This method is called on each active thread to commence root scanning. Each thread should scan its own
	 * stack to identify heap object references, as well as participate in identifying additional heap references
	 * that would not be discovered in the subsequent traversal of the object reference graph depending from the
	 * root set.
	 *
	 * For each root object identified, MM_MarkingScheme::scanObject() must be called via _markingScheme.
	 *
	 * @param env The environment for the calling thread
	 */
	void scanRoots(MM_EnvironmentBase *env);

	/**
	 * This method is called for every live object discovered during marking. It must return an object scanner instance that
	 * is appropriate for the type of object to be scanned.
	 *
	 * @param env The environment for the calling thread
	 * @param objectPtr Points to the heap object to be scanned
	 * @param reason Enumerator identifying the reason for scanning this object
	 * @param sizeToDo For pointer arrays (which may be split into multiple subsegments for parallelization), stop after scanning this many bytes
	 * @return An object scanner instance that is appropriate for the type of object to be scanned
	 *
	 * @see GC_ObjectScanner
	 * @see GC_ObjectModel::getSizeInBytesWithHeader()
	 */
	MMINLINE GC_ObjectScanner *
	getObjectScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, void *scannerSpace, MM_MarkingSchemeScanReason reason, uintptr_t *sizeToDo)
	{
		GC_MixedObjectScanner *objectScanner = GC_MixedObjectScanner::newInstance(env, objectPtr, scannerSpace, 0);
		*sizeToDo = sizeof(fomrobject_t) + objectScanner->getBytesRemaining();
		return objectScanner;
	}

	/**
	 * This method is called when a marked object cannot be enqueued on a work packet. This condition is handled
	 * primariliy by OMR but such objects are presented here in case additional language-dependent handling is
	 * required.
	 *
	 * This method is informational, no specific action is required.
	 *
	 * @param env The environment for the calling thread
	 */
	MMINLINE void handleWorkPacketOverflowItem(MM_EnvironmentBase *env, omrobjectptr_t objectPtr) { }

	/**
	 * This method is called after the object graph depending from the root set has been traversed and all live
	 * heap objects included in the root set and dependent graph have been marked. If there are other heap objects
	 * that may not have been marked in the object reference graph traversal they should be marked here and, when
	 * this additional marking is complete, MM_MarkingScheme::completeScan() must be called to mark live objects
	 * depending from additional marked objects.
	 *
	 * This method is informational, no specific action is required.
	 *
	 * @param env The environment for the calling thread
	 */
	MMINLINE void completeMarking(MM_EnvironmentBase *env) { }

	/**
	 * Constructor.
	 */
	MMINLINE MM_MarkingDelegate()
		: _objectModel(NULL)
		, _markingScheme(NULL)
	{ }
};

#endif /* MARKINGDELEGATE_HPP_ */
