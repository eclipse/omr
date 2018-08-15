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

#include "EnvironmentStandard.hpp"
#include "Evacuator.hpp"
#include "EvacuatorDelegate.hpp"
#include "ForwardedHeader.hpp"
#include "Scavenger.hpp"
#include "SublistFragment.hpp"

void
MM_EvacuatorDelegate::cycleStart()
{
	_env = _evacuator->getEnvironment();
	_isCleared = false;
}

void
MM_EvacuatorDelegate::scanRoots()
{
	OMR_VM_Example *omrVM = (OMR_VM_Example *)_env->getOmrVM()->_language_vm;
	if (_env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(_env, UNIQUE_ID)) {
		J9HashTableState state;
		if (NULL != omrVM->rootTable) {
			RootEntry *rootEntry = (RootEntry *)hashTableStartDo(omrVM->rootTable, &state);
			while (NULL != rootEntry) {
				if (NULL != rootEntry->rootPtr) {
					_evacuator->evacuateRootObject((volatile omrobjectptr_t *) &rootEntry->rootPtr);
					Debug_MM_true(_evacuator->isInSurvivor(rootEntry->rootPtr) || _evacuator->isInTenure(rootEntry->rootPtr));
				}
				rootEntry = (RootEntry *)hashTableNextDo(&state);
			}
		}
		OMR_VMThread *walkThread;
		GC_OMRVMThreadListIterator threadListIterator(_env->getOmrVM());
		while((walkThread = threadListIterator.nextOMRVMThread()) != NULL) {
			if (NULL != walkThread->_savedObject1) {
				_evacuator->evacuateRootObject((volatile omrobjectptr_t *) &walkThread->_savedObject1);
				Debug_MM_true(_evacuator->isInSurvivor((omrobjectptr_t)(walkThread->_savedObject1)) || _evacuator->isInTenure((omrobjectptr_t)(walkThread->_savedObject1)));
			}
			if (NULL != walkThread->_savedObject2) {
				_evacuator->evacuateRootObject((volatile omrobjectptr_t *) &walkThread->_savedObject2);
				Debug_MM_true(_evacuator->isInSurvivor((omrobjectptr_t)(walkThread->_savedObject2)) || _evacuator->isInTenure((omrobjectptr_t)(walkThread->_savedObject2)));
			}
		}
		_env->_currentTask->releaseSynchronizedGCThreads(_env);
	}
}

void
MM_EvacuatorDelegate::scanClearable()
{
	OMRPORT_ACCESS_FROM_OMRVM(_env->getOmrVM());
	OMR_VM_Example *omrVM = (OMR_VM_Example *)_env->getOmrVM()->_language_vm;
	if (NULL != omrVM->objectTable) {
		if (_env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(_env, UNIQUE_ID)) {
			J9HashTableState state;
			ObjectEntry *objectEntry = (ObjectEntry *)hashTableStartDo(omrVM->objectTable, &state);
			while (NULL != objectEntry) {
				if (_evacuator->isInEvacuate(objectEntry->objPtr)) {
					MM_ForwardedHeader fwdHeader(objectEntry->objPtr);
					if (fwdHeader.isForwardedPointer()) {
						objectEntry->objPtr = fwdHeader.getForwardedObject();
						Debug_MM_true(_evacuator->isInSurvivor(objectEntry->objPtr) || _evacuator->isInTenure(objectEntry->objPtr));
					} else {
						omrmem_free_memory((void *)objectEntry->name);
						objectEntry->name = NULL;
						hashTableDoRemove(&state);
					}
				}
				objectEntry = (ObjectEntry *)hashTableNextDo(&state);
			}
		}
		_isCleared = true;
		_env->_currentTask->releaseSynchronizedGCThreads(_env);
	}
}
