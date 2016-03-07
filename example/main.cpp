/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2013, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include <string.h>

#include "omrport.h"
#include "ModronAssertions.h"
/* OMR Imports */
#include "AllocateDescription.hpp"
#include "CollectorLanguageInterfaceImpl.hpp"
#include "ConfigurationLanguageInterfaceImpl.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ObjectModel.hpp"
#include "omr.h"
#include "omrgcstartup.hpp"
#include "omrvm.h"
#include "StartupManagerImpl.hpp"
#include "omrExampleVM.hpp"

extern "C" {

int
testMain(int argc, char ** argv, char **envp)
{
	/* Start up */
	OMR_VM_Example exampleVM;
	OMR_VMThread *omrVMThread = NULL;
	omrthread_t self = NULL;
	exampleVM._omrVM = NULL;
	exampleVM.rootTable = NULL;

	/* Initialize the VM */
	omr_error_t rc = OMR_Initialize(&exampleVM, &exampleVM._omrVM);
	Assert_MM_true(OMR_ERROR_NONE == rc);

	/* Recursive omrthread_attach() (i.e. re-attaching a thread that is already attached) is cheaper and less fragile
	 * than non-recursive. If performing a sequence of function calls that are likely to attach & detach internally,
	 * it is more efficient to call omrthread_attach() before the entire block.
	 */
	int j9rc = (int) omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);
	Assert_MM_true(0 == j9rc);

	/* Initialize root table */
	exampleVM.rootTable = hashTableNew(
			exampleVM._omrVM->_runtime->_portLibrary, OMR_GET_CALLSITE(), 0, sizeof(RootEntry), 0, 0, OMRMEM_CATEGORY_MM,
			rootTableHashFn, rootTableHashEqualFn, NULL, NULL);

	/* Initialize heap and collector */
	{
		/* This has to be done in local scope because MM_StartupManager has a destructor that references the OMR VM */
		MM_StartupManagerImpl startupManager(exampleVM._omrVM);
		rc = OMR_GC_IntializeHeapAndCollector(exampleVM._omrVM, &startupManager);
	}
	Assert_MM_true(OMR_ERROR_NONE == rc);

	/* Attach current thread to the VM */
	rc = OMR_Thread_Init(exampleVM._omrVM, NULL, &omrVMThread, "GCTestMainThread");
	Assert_MM_true(OMR_ERROR_NONE == rc);

	/* Kick off the dispatcher threads */
	rc = OMR_GC_InitializeDispatcherThreads(omrVMThread);
	Assert_MM_true(OMR_ERROR_NONE == rc);
	
	OMRPORT_ACCESS_FROM_OMRVM(exampleVM._omrVM);
	omrtty_printf("VM/GC INITIALIZED\n");

	/* Do stuff */

	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	MM_ObjectAllocationInterface *allocationInterface = env->_objectAllocationInterface;
	MM_GCExtensionsBase *extensions = env->getExtensions();

	omrtty_printf("configuration is %s\n", extensions->configuration->getBaseVirtualTypeId());
	omrtty_printf("collector interface is %s\n", env->getExtensions()->collectorLanguageInterface->getBaseVirtualTypeId());
	omrtty_printf("garbage collector is %s\n", env->getExtensions()->getGlobalCollector()->getBaseVirtualTypeId());
	omrtty_printf("allocation interface is %s\n", allocationInterface->getBaseVirtualTypeId());

	/* Allocate objects without collection until heap exhausted */
	uintptr_t allocatedFlags = 0;
	uintptr_t size = extensions->objectModel.adjustSizeInBytes(24);
	MM_AllocateDescription mm_allocdescription(size, allocatedFlags, true, true);
	uintptr_t allocatedCount = 0;
	while (true) {
		omrobjectptr_t obj = (omrobjectptr_t)allocationInterface->allocateObject(env, &mm_allocdescription, env->getMemorySpace(), false);
		if (NULL != obj) {
			extensions->objectModel.setObjectSize(obj, mm_allocdescription.getBytesRequested());
			RootEntry rEntry = {"root1", obj};
			RootEntry *entryInTable = (RootEntry *)hashTableAdd(exampleVM.rootTable, &rEntry);
			if (NULL == entryInTable) {
				omrtty_printf("failed to add new root to root table!\n");
			}
			/* update entry if it already exists in table */
			entryInTable->rootPtr = obj;
			allocatedCount++;
		} else {
			break;
		}
	}

	/* Print/verify thread allocation stats before GC */
	MM_AllocationStats *allocationStats = allocationInterface->getAllocationStats();
	omrtty_printf("thread allocated %d tlh bytes, %d non-tlh bytes, from %d allocations before NULL\n",
		allocationStats->tlhBytesAllocated(), allocationStats->nontlhBytesAllocated(), allocatedCount);
	uintptr_t allocationTotalBytes = allocationStats->tlhBytesAllocated() + allocationStats->nontlhBytesAllocated();
	uintptr_t allocatedTotalBytes = size * allocatedCount;
	Assert_MM_true(allocatedTotalBytes == allocationTotalBytes);

	/* Force GC to print verbose system allocation stats -- should match thread allocation stats from before GC */
	omrobjectptr_t obj = (omrobjectptr_t)allocationInterface->allocateObject(env, &mm_allocdescription, env->getMemorySpace(), true);
	env->unwindExclusiveVMAccessForGC();
	Assert_MM_false(NULL == obj);
	extensions->objectModel.setObjectSize(obj, mm_allocdescription.getBytesRequested());

	omrtty_printf("ALL TESTS PASSED\n");

	/* Shut down */

	/* Shut down the dispatcher therads */
	rc = OMR_GC_ShutdownDispatcherThreads(omrVMThread);
	Assert_MM_true(OMR_ERROR_NONE == rc);

	/* Shut down collector */
	rc = OMR_GC_ShutdownCollector(omrVMThread);
	Assert_MM_true(OMR_ERROR_NONE == rc);

	/* Detach from VM */
	rc = OMR_Thread_Free(omrVMThread);
	Assert_MM_true(OMR_ERROR_NONE == rc);

	/* Shut down heap */
	rc = OMR_GC_ShutdownHeap(exampleVM._omrVM);
	Assert_MM_true(OMR_ERROR_NONE == rc);

	/* Free root hash table */
	hashTableFree(exampleVM.rootTable);

	/* Balance the omrthread_attach_ex() issued above */
	omrthread_detach(self);

	/* Shut down VM
	 * This destroys the port library and the omrthread library.
	 * Don't use any port library or omrthread functions after this.
	 *
	 * (This also shuts down trace functionality, so the trace assertion
	 * macros might not work after this.)
	 */
	rc = OMR_Shutdown(exampleVM._omrVM);
	Assert_MM_true(OMR_ERROR_NONE == rc);

	return rc;
}

}
