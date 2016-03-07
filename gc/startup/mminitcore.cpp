/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#include "mminitcore.h"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "ModronAssertions.h"
#include "ObjectAllocationInterface.hpp"

/* OMRTODO temporary workaround to allow both ut_j9mm.h and ut_omrmm.h to be included.
 *                 Dependency on ut_j9mm.h should be removed in the future.
 */
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_omrmm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the threads mutator information (RS pointers, reference list pointers etc) for GC/MM purposes.
 *
 * @note vmThread MAY NOT be initialized completely from an execution model perspective.
 * @return 0 if OK, or non 0 if error
 */
intptr_t
initializeMutatorModel(OMR_VMThread *omrVMThread)
{
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	omrVMThread->_gcOmrVMThreadExtensions = extensions->configuration->createEnvironment(extensions, omrVMThread);
	if (NULL != omrVMThread->_gcOmrVMThreadExtensions) {
		if (extensions->isStandardGC()) {
			void *lowAddress = extensions->heapBaseForBarrierRange0;
			void *highAddress = (void *)((uintptr_t)extensions->heapBaseForBarrierRange0 + extensions->heapSizeForBarrierRange0);
			omrVMThread->lowTenureAddress = lowAddress;
			omrVMThread->highTenureAddress = highAddress;

			/* replacement values for lowTenureAddress and highTenureAddress */
			omrVMThread->heapBaseForBarrierRange0 = extensions->heapBaseForBarrierRange0;
			omrVMThread->heapSizeForBarrierRange0 = extensions->heapSizeForBarrierRange0;
		} else if (extensions->isVLHGC()) {
			MM_Heap *heap = extensions->getHeap();
			void *heapBase = heap->getHeapBase();
			void *heapTop = heap->getHeapTop();

			/* replacement values for lowTenureAddress and highTenureAddress */
			omrVMThread->heapBaseForBarrierRange0 = heapBase;
			omrVMThread->heapSizeForBarrierRange0 = (uintptr_t)heapTop - (uintptr_t)heapBase;

			/* lowTenureAddress and highTenureAddress are actually supposed to be the low and high addresses of the heap for which card
			 * dirtying is required (the JIT uses this as a range check to determine if it needs to dirty a card when writing into an
			 * object).  Setting these for Tarok is just a work-around until a more generic solution is implemented
			 */
			omrVMThread->lowTenureAddress = heapBase;
			omrVMThread->highTenureAddress = heapTop;
		}
		omrVMThread->memorySpace = extensions->heap->getDefaultMemorySpace();
	} else {
		return -1;
	}
	return 0;
}

/**
 * Cleanup Mutator specific resources (TLH, thread extension, etc) on shutdown.
 */
void
cleanupMutatorModel(OMR_VMThread *omrVMThread, uintptr_t flushCaches)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

	if (NULL != env) {
		if (flushCaches) {
			env->_objectAllocationInterface->flushCache(env);
		}
		env->kill();
	}

	omrVMThread->_gcOmrVMThreadExtensions = NULL;
}


intptr_t
gcOmrInitializeDefaults(OMR_VM* omrVM)
{
	MM_EnvironmentBase env(omrVM);

	/* allocate extension structure */

	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::newInstance(&env);
	if (NULL == extensions) {
		goto error;
	}
	extensions->setOmrVM(omrVM);
	omrVM->_gcOmrVMExtensions = (void *)extensions;

	return 0;

error:
	return -1;
}
#if defined(OMR_RAS_TDF_TRACE)
void
gcOmrInitializeTrace(OMR_VMThread *omrVMThread)
{
	UT_OMRMM_MODULE_LOADED(omrVMThread->_vm->utIntf);
}
#endif /*OMR_RAS_TDF_TRACE */

#ifdef __cplusplus
} /* extern "C" { */
#endif

