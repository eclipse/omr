/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2013, 2017
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
#include "Configuration.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "ObjectAllocationModel.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ObjectModel.hpp"
#include "omr.h"
#include "omrgc.h"
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
	exampleVM._omrVM = NULL;
	exampleVM.rootTable = NULL;
	exampleVM.objectTable = NULL;
	exampleVM._vmAccessMutex = NULL;
	exampleVM._vmExclusiveAccessCount = 0;

	/* Initialize the VM */
	omr_error_t rc = OMR_Initialize_VM(&exampleVM._omrVM, &omrVMThread, &exampleVM, NULL);
	Assert_MM_true(OMR_ERROR_NONE == rc);

	/* Set up the vm access mutex */
	intptr_t rw_rc = omrthread_rwmutex_init(&exampleVM._vmAccessMutex, 0, "VM exclusive access");
	Assert_MM_true(J9THREAD_RWMUTEX_OK == rw_rc);

	/* Initialize root table */
	exampleVM.rootTable = hashTableNew(
			exampleVM._omrVM->_runtime->_portLibrary, OMR_GET_CALLSITE(), 0, sizeof(RootEntry), 0, 0, OMRMEM_CATEGORY_MM,
			rootTableHashFn, rootTableHashEqualFn, NULL, NULL);

	/* Initialize root table */
	exampleVM.objectTable = hashTableNew(
			exampleVM._omrVM->_runtime->_portLibrary, OMR_GET_CALLSITE(), 0, sizeof(ObjectEntry), 0, 0, OMRMEM_CATEGORY_MM,
			objectTableHashFn, objectTableHashEqualFn, NULL, NULL);

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
	uintptr_t allocSize = 24;
	uintptr_t allocatedCount = 0;
	uintptr_t adjustedSize = extensions->objectModel.adjustSizeInBytes(allocSize);
	while (true) {
		MM_ObjectAllocationModel allocationModel(env, allocSize, allocatedFlags);
		omrobjectptr_t obj = (omrobjectptr_t)OMR_GC_AllocateObject(omrVMThread, &allocationModel);
		if (NULL != obj) {
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
	uintptr_t allocatedTotalBytes = adjustedSize * allocatedCount;
	Assert_MM_true(allocatedTotalBytes == allocationTotalBytes);

	/* Force GC to print verbose system allocation stats -- should match thread allocation stats from before GC */
	MM_ObjectAllocationModel allocationModel(env, allocSize, allocatedFlags);
	omrobjectptr_t obj = (omrobjectptr_t)OMR_GC_AllocateObject(omrVMThread, &allocationModel);
	Assert_MM_false(NULL == obj);

	omrtty_printf("ALL TESTS PASSED\n");

	/* Shut down */

	/* Free object hash table */
	hashTableForEachDo(exampleVM.objectTable, objectTableFreeFn, &exampleVM);
	hashTableFree(exampleVM.objectTable);
	exampleVM.objectTable = NULL;

	/* Free root hash table */
	hashTableFree(exampleVM.rootTable);
	exampleVM.rootTable = NULL;

	if (NULL != exampleVM._vmAccessMutex) {
		omrthread_rwmutex_destroy(exampleVM._vmAccessMutex);
		exampleVM._vmAccessMutex = NULL;
	}

	/* Shut down VM
	 * This destroys the port library and the omrthread library.
	 * Don't use any port library or omrthread functions after this.
	 *
	 * (This also shuts down trace functionality, so the trace assertion
	 * macros might not work after this.)
	 */
	rc = OMR_Shutdown_VM(exampleVM._omrVM, omrVMThread);
	/* Can not assert the value of rc since the portlibrary and trace engine have been shutdown */

	return rc;
}

}
