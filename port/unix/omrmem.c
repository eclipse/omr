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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Memory Utilities
 */


/*
 * This file contains code for the portability library memory management.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "omrportasserts.h"
#include "ut_omrport.h"
#include "protect_helpers.h"
#if defined(LINUX) || defined(OSX)
#include <sys/mman.h>
#endif /* defined(LINUX) || defined(OSX) */

#if defined(AIXPPC)
#include <sys/shm.h>
#include <sys/vminfo.h>
#endif/*AIXPPC*/

#if defined(J9ZOS390)

#include "omrvmem.h"
#include <sys/types.h>

#pragma linkage (PGSERRM,OS)
#pragma map(Pgser_Release,"PGSERRM")
intptr_t Pgser_Release(void *address, int byteAmount);

#if defined(OMR_ENV_DATA64)
#pragma linkage(j9discard_data,OS_NOSTACK)
int j9discard_data(void *address, int numFrames);
#endif /*OMR_ENV_DATA64 */

#endif /*J9ZOS390 */

void *
omrmem_allocate_memory_basic(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount)
{
#if (defined(S390) || defined(J9ZOS390)) && !defined(OMR_ENV_DATA64)
	return (void *)(((uintptr_t) malloc(byteAmount)) & 0x7FFFFFFF);
#elif defined(J9ZOS390) && defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	void *retval = NULL;
	retval = malloc(byteAmount);
	Assert_AddressAbove4GBBar((NULL == retval) || (0x100000000 <= ((uintptr_t)retval)));
	return retval;
#else
	return malloc(byteAmount);
#endif
}

void
omrmem_free_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer)
{
	free(memoryPointer);
}

void
omrmem_advise_and_free_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t memorySize)
{
	uintptr_t pageSize = 0;

#if defined(LINUX) || defined(OSX)
	pageSize = sysconf(_SC_PAGESIZE);
#elif defined(AIXPPC)
	struct vm_page_info pageInfo;
	pageInfo.addr = (uint64_t) portLibrary->portGlobals;
	/*  retrieve size of the page backing portLibrary->portGlobals which is allocated
	 *  using omrmem_allocate_memory() in port/unix/omrmem.c
	 */
	if (0 == vmgetinfo(&pageInfo, VM_PAGE_INFO, sizeof(struct vm_page_info))) {
		pageSize = pageInfo.pagesize;
	} else {
		/* vmgetinfo() can fail with VM_PAGE_INFO only if vm_page_info.addr is invalid,
		 * which should never happen here as portLibrary->portGlobals is valid until VM shutdown.
		 */
		Trc_PRT_mem_advise_and_free_memory_basic_vmgetinfo_failed(errno);
		Assert_PRT_ShouldNeverHappen_wrapper();
	}
#elif defined(J9ZOS390)
	pageSize = FOUR_K;
#endif /* J9ZOS390 */

	Trc_PRT_mem_advise_and_free_memory_basic_params(pageSize, memoryPointer, memorySize);

	if (pageSize > 0 && memorySize >= pageSize) {
		uintptr_t memPtrPageRounded = 0;
		uintptr_t memPtrSizePageRounded = 0;
		uintptr_t tmp = (pageSize - (((uintptr_t)memoryPointer) % pageSize));

		memPtrSizePageRounded = pageSize * ((memorySize - tmp) / pageSize);
		memPtrPageRounded = ((uintptr_t)memoryPointer) + tmp;
		/* Don't check madvise return code, because logging a tracepoint may
		 * slow things down, and the OS is free to ignore madvise info anyways.
		 */
		if (memPtrSizePageRounded >= pageSize) {

			Trc_PRT_mem_advise_and_free_memory_basic_oscall(memPtrPageRounded, memPtrSizePageRounded);

#if defined(LINUX) || defined(OSX)
			if (-1 == madvise((void *)memPtrPageRounded, memPtrSizePageRounded, MADV_DONTNEED)) {
				Trc_PRT_mem_advise_and_free_memory_basic_madvise_failed((void *)memPtrPageRounded, memPtrSizePageRounded, errno);
			}
#elif defined(AIXPPC)
			if (-1 == disclaim64((void *)memPtrPageRounded, memPtrSizePageRounded, DISCLAIM_ZEROMEM)) {
				Trc_PRT_mem_advise_and_free_memory_basic_disclaim64_failed((void *)memPtrPageRounded, memPtrSizePageRounded, errno);
				Assert_PRT_ShouldNeverHappen_wrapper();
			}
#elif defined(J9ZOS390)

#if defined(OMR_ENV_DATA64)
			if (j9discard_data((void *)memPtrPageRounded, memPtrSizePageRounded >> ZOS_REAL_FRAME_SIZE_SHIFT) != 0) {
				Trc_PRT_mem_advise_and_free_memory_basic_j9discard_data_failed((void *)memPtrPageRounded, memPtrSizePageRounded);
				Assert_PRT_ShouldNeverHappen_wrapper();
			}
#else
			if (Pgser_Release((void *)memPtrPageRounded, memPtrSizePageRounded) != 0) {
				Trc_PRT_mem_advise_and_free_memory_basic_Pgser_Release_failed((void *)memPtrPageRounded, memPtrSizePageRounded);
				Assert_PRT_ShouldNeverHappen_wrapper();
			}
#endif /* defined(OMR_ENV_DATA64) */

#endif /* defined(J9ZOS390) */
		}
	}

	free(memoryPointer);
}

void *
omrmem_reallocate_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount)
{
#if (defined(S390) || defined(J9ZOS390)) && !defined(OMR_ENV_DATA64)
	return (void *)(((uintptr_t) realloc(memoryPointer, byteAmount)) & 0x7FFFFFFF);
#else
	return realloc(memoryPointer, byteAmount);
#endif
}

void
omrmem_shutdown_basic(struct OMRPortLibrary *portLibrary)
{
#if defined(LINUX) || defined(OSX)
	omrmem_free_memory_basic(portLibrary, portLibrary->portGlobals->procSelfMap);
#endif /* defined(LINUX) || defined(OSX) */
	omrmem_free_memory_basic(portLibrary, portLibrary->portGlobals);
}

void
omrmem_startup_basic(struct OMRPortLibrary *portLibrary, uintptr_t portGlobalSize)
{
#if defined(J9ZOS390) && defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	char tmpbuff[256];
#endif

	/* omrmem_allocate_memory needs portGlobals to have been set-up */
	portLibrary->portGlobals = omrmem_allocate_memory_basic(portLibrary, portGlobalSize);
	if (NULL == portLibrary->portGlobals) {
		return;
	}
	memset(portLibrary->portGlobals, 0, portGlobalSize);
#if defined(LINUX)
	portLibrary->portGlobals->procSelfMap = omrmem_allocate_memory_basic(portLibrary, J9OSDUMP_SIZE);
#endif /* defined(LINUX) */

#if defined(J9ZOS390) && defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	/* 'disableEnsureCap32' is set using omrport_control(OMRPORT_CTLDATA_NOSUBALLOC32BITMEM,...),
	 * or by setting the J9_SUBALLOC_FOR_32 environment variable before starting the JVM.
	 *
	 * By default the sub allocator is enabled, and will not use __malloc31 on z/OS.
	 */
	portLibrary->portGlobals->disableEnsureCap32 = OMRPORT_ENABLE_ENSURE_CAP32;
	if (0 <= portLibrary->sysinfo_get_env(portLibrary, "J9_SUBALLOC_FOR_32", (void *)tmpbuff, 256)) {
		if ((0 == strncmp("FALSE", tmpbuff, 6)) || (0 == strncmp("0", tmpbuff, 2))) {
			portLibrary->portGlobals->disableEnsureCap32 = OMRPORT_DISABLE_ENSURE_CAP32;
		}
	}
#endif

}

void *
omrmem_allocate_portLibrary_basic(uintptr_t byteAmount)
{
	return (void *) malloc(byteAmount);
}

void
omrmem_deallocate_portLibrary_basic(void *memoryPointer)
{
	free(memoryPointer);
}
