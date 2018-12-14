/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

#ifndef SCAVENGERROOTSCANNER_HPP_
#define SCAVENGERROOTSCANNER_HPP_

#include "omr.h"
#include "omrcfg.h"
#include "omrExampleVM.hpp"
#include "omrhashtable.h"

#include "Base.hpp"
#include "EnvironmentStandard.hpp"
#include "ForwardedHeader.hpp"
#include "Scavenger.hpp"
#include "SublistFragment.hpp"

#if defined(OMR_GC_MODRON_SCAVENGER)

class MM_ScavengerRootScanner : public MM_Base
{
	/*
	 * Member data and types
	 */
private:
	MM_Scavenger *_scavenger;

protected:
public:

	/*
	 * Member functions
	 */
private:
protected:
public:
	MM_ScavengerRootScanner(MM_EnvironmentBase *env, MM_Scavenger *scavenger)
		: MM_Base()
		, _scavenger(scavenger)
	{
	};

	void
	scavengeRememberedSet(MM_EnvironmentStandard *env)
	{
		MM_SublistFragment::flush((J9VMGC_SublistFragment*)&env->_scavengerRememberedSet);
		_scavenger->scavengeRememberedSet(env);
	}

	void
	pruneRememberedSet(MM_EnvironmentStandard *env)
	{
		_scavenger->pruneRememberedSet(env);
	}

	void
	scanRoots(MM_EnvironmentBase *env)
	{
		OMR_VM_Example *omrVM = (OMR_VM_Example *)env->getOmrVM()->_language_vm;
		if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
			J9HashTableState state;
			MM_EnvironmentStandard *envStd = MM_EnvironmentStandard::getEnvironment(env);
			if (NULL != omrVM->rootTable) {
				RootEntry *rootEntry = (RootEntry *)hashTableStartDo(omrVM->rootTable, &state);
				while (NULL != rootEntry) {
					if (NULL != rootEntry->rootPtr) {
						_scavenger->copyObjectSlot(envStd, (volatile omrobjectptr_t *) &rootEntry->rootPtr);
					}
					rootEntry = (RootEntry *)hashTableNextDo(&state);
				}
			}
			OMR_VMThread *walkThread;
			GC_OMRVMThreadListIterator threadListIterator(env->getOmrVM());
			while((walkThread = threadListIterator.nextOMRVMThread()) != NULL) {
				if (NULL != walkThread->_savedObject1) {
					_scavenger->copyObjectSlot(envStd, (volatile omrobjectptr_t *) &walkThread->_savedObject1);
				}
				if (NULL != walkThread->_savedObject2) {
					_scavenger->copyObjectSlot(envStd, (volatile omrobjectptr_t *) &walkThread->_savedObject2);
				}
			}
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
	}

	void rescanThreadSlots(MM_EnvironmentStandard *env) { }

	void scanClearable(MM_EnvironmentBase *env)
	{
		OMRPORT_ACCESS_FROM_OMRVM(env->getOmrVM());
		OMR_VM_Example *omrVM = (OMR_VM_Example *)env->getOmrVM()->_language_vm;
		if (NULL != omrVM->objectTable) {
			if (env->_currentTask->synchronizeGCThreadsAndReleaseSingleThread(env, UNIQUE_ID)) {
				J9HashTableState state;
				ObjectEntry *objectEntry = (ObjectEntry *)hashTableStartDo(omrVM->objectTable, &state);
				while (NULL != objectEntry) {
					if (_scavenger->isObjectInEvacuateMemory(objectEntry->objPtr)) {
						MM_ForwardedHeader fwdHeader(objectEntry->objPtr);
						if (fwdHeader.isForwardedPointer()) {
							objectEntry->objPtr = fwdHeader.getForwardedObject();
						} else {
							omrmem_free_memory((void *)objectEntry->name);
							objectEntry->name = NULL;
							hashTableDoRemove(&state);
						}
					}
					objectEntry = (ObjectEntry *)hashTableNextDo(&state);
				}
			}
			env->_currentTask->releaseSynchronizedGCThreads(env);
		}
	}

	void flush(MM_EnvironmentStandard *env) { }
};

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#endif /* SCAVENGERROOTSCANNER_HPP_ */
