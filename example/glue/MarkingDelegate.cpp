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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "omr.h"
#include "omrhashtable.h"

#include "EnvironmentBase.hpp"
#include "MarkingScheme.hpp"
#include "omrExampleVM.hpp"
#include "OMRVMThreadListIterator.hpp"

#include "MarkingDelegate.hpp"

void
MM_MarkingDelegate::scanRoots(MM_EnvironmentBase *env)
{
	OMR_VM_Example *omrVM = (OMR_VM_Example *)env->getOmrVM()->_language_vm;
	J9HashTableState state;
	RootEntry *rEntry = NULL;
	rEntry = (RootEntry *)hashTableStartDo(omrVM->rootTable, &state);
	while (rEntry != NULL) {
		_markingScheme->markObject(env, rEntry->rootPtr);
		rEntry = (RootEntry *)hashTableNextDo(&state);
	}
	OMR_VMThread *walkThread;
	GC_OMRVMThreadListIterator threadListIterator(env->getOmrVM());
	while((walkThread = threadListIterator.nextOMRVMThread()) != NULL) {
		if (NULL != walkThread->_savedObject1) {
			_markingScheme->markObject(env, (omrobjectptr_t)walkThread->_savedObject1);
		}
		if (NULL != walkThread->_savedObject2) {
			_markingScheme->markObject(env, (omrobjectptr_t)walkThread->_savedObject2);
		}
	}
}

void
MM_MarkingDelegate::masterCleanupAfterGC(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRVM(env->getOmrVM());
	J9HashTableState state;
	ObjectEntry *objEntry = NULL;
	OMR_VM_Example *omrVM = (OMR_VM_Example *)env->getOmrVM()->_language_vm;
	objEntry = (ObjectEntry *)hashTableStartDo(omrVM->objectTable, &state);
	while (objEntry != NULL) {
		if (!_markingScheme->isMarked(objEntry->objPtr)) {
			omrmem_free_memory((void *)objEntry->name);
			objEntry->name = NULL;
			hashTableDoRemove(&state);
		}
		objEntry = (ObjectEntry *)hashTableNextDo(&state);
	}
}
