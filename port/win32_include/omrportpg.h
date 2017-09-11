/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef omrportpg_h
#define omrportpg_h

/*
 * @ddr_namespace: default
 */

#include "omrcfg.h"
#include "omrthread.h"
#include "pool_api.h"

#include "avl_api.h"

#if defined(OMR_ENV_DATA64)
#include "omrmem32struct.h"
#endif

#include <DbgHelp.h>
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>

/** Number of pageSizes supported.  There is always 1 for the default size, and 1 for the 0 terminator.
 * The number of large pages supported determines the remaining size.
 * Responsibility of the implementation of omrvmem to initialize this table correctly.
 */
#define OMRPORT_VMEM_PAGESIZE_COUNT 3

/* debug library references */
typedef LPAPI_VERSION(__stdcall *IMAGEHLPAPIVERSION)(void);
typedef BOOL (__stdcall *SYMINITIALIZEW)(HANDLE, PWSTR, BOOL);
typedef BOOL (__stdcall *SYMCLEANUP)(HANDLE hProcess);
typedef BOOL (__stdcall *SYMCLEANUP)(HANDLE);
typedef DWORD (__stdcall *SYMGETOPTIONS)(void);
typedef DWORD (__stdcall *SYMSETOPTIONS)(DWORD);
typedef BOOL (__stdcall *SYMFROMADDR)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
typedef BOOL (__stdcall *STACKWALK64)(DWORD
									  , HANDLE
									  , HANDLE
									  , LPSTACKFRAME64
									  , PVOID
									  , PREAD_PROCESS_MEMORY_ROUTINE64
									  , PFUNCTION_TABLE_ACCESS_ROUTINE64
									  , PGET_MODULE_BASE_ROUTINE64
									  , PTRANSLATE_ADDRESS_ROUTINE64
									 );

typedef DWORD64 (__stdcall *SYMGETMODULEBASE64)(HANDLE, DWORD64);
typedef PVOID (__stdcall *SYMFUNCTIONTABLEACCESS64)(HANDLE, DWORD64);
typedef BOOL (__stdcall *SYMGETLINEFROMADDR64)(HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line);
typedef BOOL (__stdcall *SYMGETMODULEINFO64)(HANDLE hProcess, DWORD64 dwAddr, PIMAGEHLP_MODULE64 ModuleInfo);

/* Winnt.h in Windows 7 or Windows Server 2008 R2 */
#if _MSC_VER < 1600
typedef struct _GROUP_AFFINITY {
	KAFFINITY Mask;
	WORD      Group;
	WORD      Reserved[3];
} GROUP_AFFINITY, *PGROUP_AFFINITY;

typedef struct _PROCESSOR_NUMBER {
	WORD Group;
	BYTE Number;
	BYTE Reserved;
} PROCESSOR_NUMBER, *PPROCESSOR_NUMBER;
#endif


typedef BOOL (__stdcall *GetThreadIdealProcessorExType)(HANDLE, PPROCESSOR_NUMBER);
typedef BOOL (__stdcall *SetThreadIdealProcessorExType)(HANDLE, PPROCESSOR_NUMBER, PPROCESSOR_NUMBER);
typedef BOOL (__stdcall *GetNumaNodeProcessorMaskExType)(USHORT, PGROUP_AFFINITY);

typedef struct _dbg_entrypoints {
	HINSTANCE                hDbgHelpLib;
	IMAGEHLPAPIVERSION       ImagehlpApiVersion;
	SYMGETOPTIONS            SymGetOptions;
	SYMSETOPTIONS            SymSetOptions;
	SYMINITIALIZEW           SymInitializeW;
	SYMCLEANUP               SymCleanup;
	SYMGETMODULEBASE64       SymGetModuleBase64;
	STACKWALK64              StackWalk64;
	SYMFUNCTIONTABLEACCESS64 SymFunctionTableAccess64;
	SYMFROMADDR              SymFromAddr;
	SYMGETLINEFROMADDR64     SymGetLineFromAddr64;
	SYMGETMODULEINFO64       SymGetModuleInfo64;
} Dbg_Entrypoints;

typedef struct _dbg_pdbPath {
	DWORD numberOfModules;
	wchar_t searchPath[4096];
	DWORD symOptions;
} dBG_pdbPath;

/* from windef.h */
typedef void *HANDLE;

#define MANAGEMENT_COUNTER_PATH_UNINITIALIZED 0
#define MANAGEMENT_COUNTER_PATH_INITIALIZED 1
#define MANAGEMENT_COUNTER_USE_OLD_ALGORITHM 2

typedef struct OMRPortPlatformGlobals {
	HANDLE tty_consoleInputHd;
	HANDLE tty_consoleOutputHd;
	HANDLE tty_consoleErrorHd;
	HANDLE mem_heap;
	uintptr_t vmem_pageSize[OMRPORT_VMEM_PAGESIZE_COUNT]; /** <0 terminated array of supported page sizes */
	uintptr_t vmem_pageFlags[OMRPORT_VMEM_PAGESIZE_COUNT]; /** <0 terminated array of flags describing type of the supported page sizes */
	char *si_osType;
	char *si_osTypeOnHeap;
	char *si_osVersion;
	uint64_t time_hiresClockFrequency;
	void *tty_consoleEventBuffer;  /* windows.h cannot be included here and it has a complex definition so use void * in place of INPUT_RECORD* */
	omrthread_monitor_t tty_consoleBufferMonitor;
	HANDLE osBacktrace_Mutex;
#if defined(OMR_ENV_DATA64)
	J9SubAllocateHeapMem32 subAllocHeapMem32;
#endif
	HANDLE hLoggingEventSource;
	Dbg_Entrypoints *dbgHlpLibraryFunctions;
	dBG_pdbPath pdbData;
	void *dbgAllocator;
	uintptr_t systemLoggingFlags;
	GetThreadIdealProcessorExType GetThreadIdealProcessorExProc;	/**< Cached pointer to GetThreadIdealProcessorEx function (or NULL if not supported on host OS level) */
	SetThreadIdealProcessorExType SetThreadIdealProcessorExProc;	/**< Cached pointer to SetThreadIdealProcessorEx function (or NULL if not supported on host OS level) */
	GetNumaNodeProcessorMaskExType GetNumaNodeProcessorMaskExProc;	/**< Cached pointer to GetNumaNodeProcessorMaskEx function (or NULL if not supported on host OS level) */
	J9AVLTree bindingTree;	/**< The tree of all tracked NUMA memory extents for all reserved memory extents - Should ideally be moved into VMIdentifier and tracked on a per-extent basis */
	J9Pool *bindingPool;	/**< The pool used for allocating the nodes which are stored in the bindingTree */
	omrthread_monitor_t bindingAccessMonitor;
	uintptr_t numa_platform_supports_numa;
	BOOLEAN vmem_initialized;
	char *si_executableName;
	char managementCounterPath[PDH_MAX_COUNTER_PATH];		/* List of performance counters */
	uintptr_t managementCounterPathStatus;					/* If counter path has been initialized or PDH query has failed */
	MUTEX managementDataLock;
} OMRPortPlatformGlobals;

#define PPG_tty_consoleInputHd	(portLibrary->portGlobals->platformGlobals.tty_consoleInputHd)
#define PPG_tty_consoleOutputHd (portLibrary->portGlobals->platformGlobals.tty_consoleOutputHd)
#define PPG_tty_consoleErrorHd	(portLibrary->portGlobals->platformGlobals.tty_consoleErrorHd)
#define PPG_mem_heap (portLibrary->portGlobals->platformGlobals.mem_heap)
#define PPG_vmem_pageSize (portLibrary->portGlobals->platformGlobals.vmem_pageSize)
#define PPG_vmem_pageFlags (portLibrary->portGlobals->platformGlobals.vmem_pageFlags)
#define PPG_si_osType (portLibrary->portGlobals->platformGlobals.si_osType)
#define PPG_si_osTypeOnHeap (portLibrary->portGlobals->platformGlobals.si_osTypeOnHeap)
#define PPG_si_osVersion (portLibrary->portGlobals->platformGlobals.si_osVersion)
#define PPG_time_hiresClockFrequency (portLibrary->portGlobals->platformGlobals.time_hiresClockFrequency)
#define PPG_tty_consoleEventBuffer (portLibrary->portGlobals->platformGlobals.tty_consoleEventBuffer)
#define PPG_tty_consoleBufferMonitor (portLibrary->portGlobals->platformGlobals.tty_consoleBufferMonitor)
#define PPG_osBacktrace_Mutex (portLibrary->portGlobals->platformGlobals.osBacktrace_Mutex)
#define PPG_managementDataLock (portLibrary->portGlobals->platformGlobals.managementDataLock)
#define PPG_managementCounterPath (portLibrary->portGlobals->platformGlobals.managementCounterPath)
#define PPG_managementCounterPathStatus (portLibrary->portGlobals->platformGlobals.managementCounterPathStatus)

#if defined(OMR_ENV_DATA64)
#define PPG_mem_mem32_subAllocHeapMem32 (portLibrary->portGlobals->platformGlobals.subAllocHeapMem32)
#endif
#define PPG_numa_platform_supports_numa (portLibrary->portGlobals->platformGlobals.numa_platform_supports_numa)
#define PPG_syslog_handle (portLibrary->portGlobals->platformGlobals.hLoggingEventSource)
#define PPG_syslog_flags (portLibrary->portGlobals->platformGlobals.systemLoggingFlags)
#define PPG_dbgHlpLibraryFunctions (portLibrary->portGlobals->platformGlobals.dbgHlpLibraryFunctions)
#define PPG_pdbData (portLibrary->portGlobals->platformGlobals.pdbData)
#define PPG_dbgAllocator (portLibrary->portGlobals->platformGlobals.dbgAllocator)
#define PPG_bindingAccessMonitor (portLibrary->portGlobals->platformGlobals.bindingAccessMonitor)
#define PPG_si_executableName (portLibrary->portGlobals->platformGlobals.si_executableName)
#endif /* omrportpg_h */
