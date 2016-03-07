/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 * @brief CPU Control.
 *
 * Functions setting CPU attributes.
 */


#include <stdlib.h>
#if defined(RS6000) || defined (LINUXPPC) || defined (PPC)
#include <string.h>
#endif
#include "omrport.h"
#if defined(RS6000) || defined (LINUXPPC) || defined (PPC)
#include "omrportpriv.h"
#include "omrportpg.h"
#endif
#if (__IBMC__ || __IBMCPP__)
void dcbf(unsigned char *);
void dcbst(unsigned char *);
void icbi(unsigned char *);
void sync(void);
void isync(void);
void dcbz(void *);
#pragma mc_func dcbf  {"7c0018ac"}
#pragma mc_func dcbst {"7c00186c"}
#pragma mc_func icbi  {"7c001fac"}
#pragma mc_func sync  {"7c0004ac"}
#pragma mc_func isync {"4c00012c"}
#pragma mc_func dcbz {"7c001fec"}
#pragma reg_killed_by dcbf
#pragma reg_killed_by dcbst
#pragma reg_killed_by dcbz
#pragma reg_killed_by icbi
#pragma reg_killed_by sync
#pragma reg_killed_by isync
#endif



/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the exit operations may be created here.  All resources created here should be destroyed
 * in @ref omrcpu_shutdown.
 *
 * @param[in] portLibrary The port library
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_CPU
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrcpu_startup(struct OMRPortLibrary *portLibrary)
{
	/* initialize the ppc level 1 cache line size */
#if defined(RS6000) || defined (LINUXPPC) || defined (PPC)
	int32_t ppcCacheLineSize;

	int  i;
	int input1 = 20;
	char buf[1024];
	memset(buf, 255, 1024);

#if (__IBMC__ || __IBMCPP__)
	dcbz((void *) &buf[512]);
#elif defined(LINUX) || defined(OSX)
	__asm__(
		"dcbz 0, %0"
		: /* no outputs */
		:"r"((void *) &buf[512]));
#endif


	for (i = 0, ppcCacheLineSize = 0; i < 1024; i++) {
		if (buf[i] == 0) {
			ppcCacheLineSize++;
		}
	}

	PPG_mem_ppcCacheLineSize = ppcCacheLineSize;
#endif

	return 0;
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrcpu_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library
 *
 * @note Most implementations will be empty.
 */
void
omrcpu_shutdown(struct OMRPortLibrary *portLibrary)
{
}

/**
 * @brief CPU Control operations.
 *
 * Flush the instruction cache to memory.
 *
 * @param[in] portLibrary The port library
 * @param[in] memoryPointer The base address of memory to flush.
 * @param[in] byteAmount Number of bytes to flush.
 */
void
omrcpu_flush_icache(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount)
{
#if defined(RS6000) || defined (LINUXPPC) || defined (PPC)

	int32_t cacheLineSize = PPG_mem_ppcCacheLineSize;
	unsigned char  *addr;
	unsigned char  *limit;
	limit = (unsigned char *)(((unsigned long)memoryPointer + (unsigned int)byteAmount + (cacheLineSize - 1))
							  / cacheLineSize * cacheLineSize);

	/* for each cache line, do a data cache block flush */
	for (addr = (unsigned char *)memoryPointer ; addr < limit; addr += cacheLineSize) {


#if (__IBMC__ || __IBMCPP__)
		dcbst(addr);
#elif defined(LINUX) || defined(OSX)
		__asm__(
			"dcbst 0,%0"
			: /* no outputs */
			: "r"(addr));
#endif
	}

#if (__IBMC__ || __IBMCPP__)
	sync();
#elif defined(LINUX) || defined(OSX)
	__asm__("sync");
#endif

	/* for each cache line  do an icache block invalidate */
	for (addr = (unsigned char *)memoryPointer; addr < limit; addr += cacheLineSize) {

#if (__IBMC__ || __IBMCPP__)
		icbi(addr);
#elif defined(LINUX) || defined(OSX)
		__asm__(
			"icbi 0,%0"
			: /* no outputs */
			: "r"(addr));
#endif
	}

#if (__IBMC__ || __IBMCPP__)
	sync();
	isync();
#elif defined(LINUX) || defined(OSX)
	__asm__("sync");
	__asm__("isync");
#endif

#endif /*  defined(RS6000) || defined (LINUXPPC) || defined (PPC) */

}

int32_t
omrcpu_get_cache_line_size(struct OMRPortLibrary *portLibrary, int32_t *lineSize)
{
	int32_t rc = OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
#if defined(RS6000) || defined (LINUXPPC) || defined (PPC)
	if (NULL != lineSize) {
		*lineSize = PPG_mem_ppcCacheLineSize;
		rc = 0;
	} else {
		rc = OMRPORT_ERROR_INVALID_ARGUMENTS;
	}
#endif /* defined(RS6000) || defined (LINUXPPC) || defined (PPC) */
	return rc;
}




