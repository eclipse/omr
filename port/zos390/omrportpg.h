/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#ifndef omrportpg_h
#define omrportpg_h

/*
 * @ddr_namespace: default
 */

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

typedef struct OMRSTFLEFacilities {
	uint64_t dw1;
	uint64_t dw2;
	uint64_t dw3;
} OMRSTFLEFacilities;

typedef struct OMRSTFLECache {
	uintptr_t lastDoubleWord;
	OMRSTFLEFacilities facilities;
} OMRSTFLECache;

typedef struct OMRPortPlatformGlobals {
	char *si_osType;
	char *si_osVersion;
	uintptr_t vmem_pageSize[OMRPORT_VMEM_PAGESIZE_COUNT]; /** <0 terminated array of supported page sizes */
	uintptr_t vmem_pageFlags[OMRPORT_VMEM_PAGESIZE_COUNT]; /** <0 terminated array of flags describing type of the supported page sizes */
	BOOLEAN isRunningInContainer;
#if defined(OMR_ENV_DATA64)
	J9SubAllocateHeapMem32 subAllocHeapMem32;
#endif
	/* This must only be set in omrsignal_startup() so that subsequent reads of this don't need to worry about synchronization */
	BOOLEAN resumableTrapsSupported;
	/* This must only be set in omrvmem_startup() so that subsequent reads of this don't need to worry about synchronization */
	uintptr_t userExtendedPrivateAreaMemoryType;
	BOOLEAN loggingEnabled;
	BOOLEAN fileTextIconvOpenFailed; /* this gets implicitly initialized to FALSE when the globals are allocated. */
	BOOLEAN globalConverterEnabled;
	iconv_t globalConverter[UNCACHED_ICONV_DESCRIPTOR];
	MUTEX globalConverterMutex[UNCACHED_ICONV_DESCRIPTOR];
	uintptr_t systemLoggingFlags;
	char *si_executableName;
	OMRSTFLECache stfleCache;
#if defined(OMR_ENV_DATA64)
	char iptTtoken[TTOKEN_BUF_SZ];
#endif /* defined(OMR_ENV_DATA64) */
} OMRPortPlatformGlobals;

#define PPG_si_osType (portLibrary->portGlobals->platformGlobals.si_osType)
#define PPG_si_osVersion (portLibrary->portGlobals->platformGlobals.si_osVersion)
#define PPG_vmem_pageSize (portLibrary->portGlobals->platformGlobals.vmem_pageSize)
#define PPG_vmem_pageFlags (portLibrary->portGlobals->platformGlobals.vmem_pageFlags)
#define PPG_isRunningInContainer (portLibrary->portGlobals->platformGlobals.isRunningInContainer)
#if defined(OMR_ENV_DATA64)
#define PPG_mem_mem32_subAllocHeapMem32 (portLibrary->portGlobals->platformGlobals.subAllocHeapMem32)
#endif
#define PPG_resumableTrapsSupported	(portLibrary->portGlobals->platformGlobals.resumableTrapsSupported)
#define PPG_userExtendedPrivateAreaMemoryType (portLibrary->portGlobals->platformGlobals.userExtendedPrivateAreaMemoryType)
#define PPG_syslog_enabled (portLibrary->portGlobals->platformGlobals.loggingEnabled)
#define PPG_syslog_flags (portLibrary->portGlobals->platformGlobals.systemLoggingFlags)
#define PPG_file_text_iconv_open_failed (portLibrary->portGlobals->platformGlobals.fileTextIconvOpenFailed)
#define PPG_global_converter_enabled (portLibrary->portGlobals->platformGlobals.globalConverterEnabled)
#define PPG_global_converter (portLibrary->portGlobals->platformGlobals.globalConverter)
#define PPG_global_converter_mutex (portLibrary->portGlobals->platformGlobals.globalConverterMutex)

#define PPG_si_executableName (portLibrary->portGlobals->platformGlobals.si_executableName)
#define PPG_ipt_ttoken (portLibrary->portGlobals->platformGlobals.iptTtoken)

#define PPG_stfleCache (portLibrary->portGlobals->platformGlobals.stfleCache)

#endif /* omrportpg_h */
