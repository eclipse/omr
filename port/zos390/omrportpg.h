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

#ifndef omrportpg_h
#define omrportpg_h

#include "omrcfg.h"
#if defined(OMR_ENV_DATA64)
#include "omrmem32struct.h"
#endif

#include "omriconvhelpers.h"

/** Number of pageSizes supported.  There is always 1 for the default size, and 1 for the 0 terminator.
 * The number of large pages supported determines the remaining size.
 * Responsibility of the implementation of omrvmem to initialize this table correctly.
 * z/OS supports following large page size:
 * 	- 2GB fixed pages
 * 	- 1MB fixed pages
 * 	- 1MB pageable pages
 */
#define OMRPORT_VMEM_PAGESIZE_COUNT 5

#if defined(OMR_ENV_DATA64)
#define TTOKEN_BUF_SZ                   16
#endif /* defined(OMR_ENV_DATA64) */

typedef struct OMRPortPlatformGlobals {
	char *si_osType;
	char *si_osVersion;
	uintptr_t vmem_pageSize[OMRPORT_VMEM_PAGESIZE_COUNT]; /** <0 terminated array of supported page sizes */
	uintptr_t vmem_pageFlags[OMRPORT_VMEM_PAGESIZE_COUNT]; /** <0 terminated array of flags describing type of the supported page sizes */
#if defined(OMR_ENV_DATA64)
	J9SubAllocateHeapMem32 subAllocHeapMem32;
#endif
	/* This must only be set in omrsignal_startup() so that subsequent reads of this don't need to worry about synchronization */
	BOOLEAN resumableTrapsSupported;
	/* This must only be set in omrvmem_startup() so that subsequent reads of this don't need to worry about synchronization */
	BOOLEAN twoToThirtyTwoGigAreaSupported;
	BOOLEAN loggingEnabled;
	BOOLEAN fileTextIconvOpenFailed; /* this gets implicitly initialized to FALSE when the globals are allocated. */
	BOOLEAN globalConverterEnabled;
	iconv_t globalConverter[UNCACHED_ICONV_DESCRIPTOR];
	MUTEX globalConverterMutex[UNCACHED_ICONV_DESCRIPTOR];
	uintptr_t systemLoggingFlags;
	char *si_executableName;
#if defined(OMR_ENV_DATA64)
	char iptTtoken[TTOKEN_BUF_SZ];
#endif /* defined(OMR_ENV_DATA64) */
} OMRPortPlatformGlobals;

#define PPG_si_osType (portLibrary->portGlobals->platformGlobals.si_osType)
#define PPG_si_osVersion (portLibrary->portGlobals->platformGlobals.si_osVersion)
#define PPG_vmem_pageSize (portLibrary->portGlobals->platformGlobals.vmem_pageSize)
#define PPG_vmem_pageFlags (portLibrary->portGlobals->platformGlobals.vmem_pageFlags)
#if defined(OMR_ENV_DATA64)
#define PPG_mem_mem32_subAllocHeapMem32 (portLibrary->portGlobals->platformGlobals.subAllocHeapMem32)
#endif
#define PPG_resumableTrapsSupported	(portLibrary->portGlobals->platformGlobals.resumableTrapsSupported)
#define PPG_twoToThirtyTwoGigAreaSupported (portLibrary->portGlobals->platformGlobals.twoToThirtyTwoGigAreaSupported)
#define PPG_syslog_enabled (portLibrary->portGlobals->platformGlobals.loggingEnabled)
#define PPG_syslog_flags (portLibrary->portGlobals->platformGlobals.systemLoggingFlags)
#define PPG_file_text_iconv_open_failed (portLibrary->portGlobals->platformGlobals.fileTextIconvOpenFailed)
#define PPG_global_converter_enabled (portLibrary->portGlobals->platformGlobals.globalConverterEnabled)
#define PPG_global_converter (portLibrary->portGlobals->platformGlobals.globalConverter)
#define PPG_global_converter_mutex (portLibrary->portGlobals->platformGlobals.globalConverterMutex)

#define PPG_si_executableName (portLibrary->portGlobals->platformGlobals.si_executableName)
#define PPG_ipt_ttoken (portLibrary->portGlobals->platformGlobals.iptTtoken)

#endif /* omrportpg_h */
