/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef SCAVENGERBACKOUTSCANNER_HPP_
#define SCAVENGERBACKOUTSCANNER_HPP_

#include "omr.h"
#include "omrcfg.h"
#include "omrExampleVM.hpp"
#include "omrhashtable.h"

#include "Base.hpp"
#include "EnvironmentStandard.hpp"
#include "Scavenger.hpp"

#if defined(OMR_GC_MODRON_SCAVENGER)

class MM_ScavengerBackOutScanner : public MM_Base
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
	MM_ScavengerBackOutScanner(MM_EnvironmentBase *env, bool singleThread, MM_Scavenger *scavenger)
		: MM_Base()
		, _scavenger(scavenger)
	{
	};

	void
	scanAllSlots(MM_EnvironmentBase *env)
	{
		J9HashTableState state;
		OMR_VM_Example *omrVM = (OMR_VM_Example *)env->getOmrVM()->_language_vm;
		RootEntry *rootEntry = (RootEntry *)hashTableStartDo(omrVM->rootTable, &state);
		while (rootEntry != NULL) {
			_scavenger->backOutFixSlotWithoutCompression((volatile omrobjectptr_t *) &rootEntry->rootPtr);
			rootEntry = (RootEntry *)hashTableNextDo(&state);
		}
		ObjectEntry *objectEntry = (ObjectEntry *)hashTableStartDo(omrVM->objectTable, &state);
		while (NULL != objectEntry) {
			if (NULL != objectEntry->objPtr) {
				_scavenger->backOutFixSlotWithoutCompression((volatile omrobjectptr_t *) &objectEntry->objPtr);
			}
			objectEntry = (ObjectEntry *)hashTableNextDo(&state);
		}
		OMR_VMThread *walkThread;
		GC_OMRVMThreadListIterator threadListIterator(env->getOmrVM());
		while((walkThread = threadListIterator.nextOMRVMThread()) != NULL) {
			if (NULL != walkThread->savedObject) {
				_scavenger->backOutFixSlotWithoutCompression((volatile omrobjectptr_t *) &walkThread->savedObject);
			}
		}
	}
};

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
#endif /* SCAVENGERBACKOUTSCANNER_HPP_ */
