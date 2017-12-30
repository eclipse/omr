/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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
/**
 * @file
 * @ingroup Port
 * @brief System information
 */

#include <pdh.h>
#include <pdhmsg.h>
#include <stdio.h>
#include <windows.h>
#include <WinSDKVer.h>

#if defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE)
#include <VersionHelpers.h>
#endif

#include "omrportpriv.h"
#include "omrportpg.h"
#include "omrportptb.h"
#include "ut_omrport.h"

static int32_t copyEnvToBuffer(struct OMRPortLibrary *portLibrary, void *args);

typedef struct CopyEnvToBufferArgs {
	uintptr_t bufferSizeBytes;
	void *buffer;
	uintptr_t numElements;
} CopyEnvToBufferArgs;

/* Missing from the ALPHA include files */
#ifndef VER_PLATFORM_WIN32_WINDOWS
#define VER_PLATFORM_WIN32_WINDOWS 1
#endif

void
omrsysinfo_set_number_entitled_CPUs(struct OMRPortLibrary *portLibrary, uintptr_t number)
{
	Trc_PRT_sysinfo_set_number_entitled_CPUs_Entered();
	portLibrary->portGlobals->entitledCPUs = number;
	Trc_PRT_sysinfo_set_number_entitled_CPUs_Exit(number);
	return;
}

/**
 * Determine the CPU architecture.
 *
 * @param[in] portLibrary The port library.
 *
 * @return A null-terminated string describing the CPU architecture of the hardware, NULL on error.
 *
 * @note portLibrary is responsible for allocation/deallocation of returned buffer.
 * @note See http://www.tolstoy.com/samizdat/sysprops.html for good values to return.
 */
const char *
omrsysinfo_get_CPU_architecture(struct OMRPortLibrary *portLibrary)
{
#if defined(_PPC_)
	return OMRPORT_ARCH_PPC;
#elif defined(_X86_)
	return OMRPORT_ARCH_X86;
#elif defined( _AMD64_)
	return OMRPORT_ARCH_HAMMER;
#else
	return "unknown";
#endif
}

#define ENVVAR_VALUE_BUFFER_LENGTH 512
#define ENVVAR_NAME_BUFFER_LENGTH 128
intptr_t
omrsysinfo_get_env(struct OMRPortLibrary *portLibrary, const char *envVar, char *infoString, uintptr_t bufSize)
{
	DWORD rc = 0;
	intptr_t result = -1;
	wchar_t envVarWideCharValueBuffer[ENVVAR_VALUE_BUFFER_LENGTH];
	wchar_t *envVarWideCharValue = envVarWideCharValueBuffer;
	wchar_t	*envVarWideCharName = NULL;
	wchar_t envVarNameConversionBuffer[ENVVAR_NAME_BUFFER_LENGTH];

	/*
	 * Convert the envvar from modified UTF-8 to UTF-16 (wide character).
	 * Stack-allocate a buffer large enough to hold typical envvar names.
	 * If the name is larger than the buffer, a new buffer is dynamically allocated.
	 */
	envVarWideCharName = port_convertFromUTF8(portLibrary, envVar, envVarNameConversionBuffer, ENVVAR_NAME_BUFFER_LENGTH);
	if (NULL == envVarWideCharName) {
		return -1;
	}
	rc = GetEnvironmentVariableW(envVarWideCharName, envVarWideCharValue, ENVVAR_VALUE_BUFFER_LENGTH); /* Try with the stack buffer first */
	if ((ENVVAR_VALUE_BUFFER_LENGTH <= rc) && (0 != rc)) {
		/* if the value fit, rc, which does not include the null, is at most ENVVAR_VALUE_BUFFER_LENGTH-1. */
		DWORD envVarValueLength = rc;
		envVarWideCharValue = (wchar_t *)portLibrary->mem_allocate_memory(portLibrary, envVarValueLength * sizeof(wchar_t), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL != envVarWideCharValue) {
			rc = GetEnvironmentVariableW(envVarWideCharName, envVarWideCharValue, envVarValueLength);
		} else {
			rc = 0; /* Memory allocation failure */
		}
	}

	if (envVarNameConversionBuffer != envVarWideCharName) {
		/* port_convertFromUTF8 allocated a buffer for us */
		portLibrary->mem_free_memory(portLibrary, envVarWideCharName);
	}

	if (0 == rc) {
		/*
		 * Possible causes:
		 * - envvar does not exist or is empty
		 * - memory allocation failure
		 */
		result = -1;
	} else {
		/* Calculate the number of bytes required for the conversion */
		rc = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, envVarWideCharValue, -1, NULL, 0, NULL, NULL);
		if (0 == rc) { /* error, probably bogus Unicode */
			result = -1;
		} else if (rc > bufSize) {
			/* Caller-supplied buffer is too small. Return the actual number of bytes required. */
			result = rc;
		} else {
			/* Do the conversion for real. */
			rc = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, envVarWideCharValue, -1,  infoString, (int)bufSize, NULL, NULL);
			Assert_PRT_true(0 != rc); /* Bogus Unicode or buffer too small should be caught by previous tests */
			result = 0;
		}
	}
	if (envVarWideCharValue != envVarWideCharValueBuffer) {
		/* We had to allocate a large buffer */
		portLibrary->mem_free_memory(portLibrary, envVarWideCharValue);
	}
	return result;

}
/**
 * Determine the OS type.
 *
 * @param[in] portLibrary The port library.
 *
 * @return OS type string (NULL terminated) on success, NULL on error.
 *
 * @note portLibrary is responsible for allocation/deallocation of returned buffer.
 */
const char *
omrsysinfo_get_OS_type(struct OMRPortLibrary *portLibrary)
{

/*
WIN32_WINNT version constants :

#define _WIN32_WINNT_NT4                    0x0400
#define _WIN32_WINNT_WIN2K                  0x0500
#define _WIN32_WINNT_WINXP                  0x0501
#define _WIN32_WINNT_WS03                   0x0502
#define _WIN32_WINNT_WIN6                   0x0600
#define _WIN32_WINNT_VISTA                  0x0600
#define _WIN32_WINNT_WS08                   0x0600
#define _WIN32_WINNT_LONGHORN               0x0600
#define _WIN32_WINNT_WIN7                   0x0601
#define _WIN32_WINNT_WIN8                   0x0602
#define _WIN32_WINNT_WINBLUE                0x0603
#define _WIN32_WINNT_WINTHRESHOLD           0x0A00 /* ABRACADABRA_THRESHOLD * /
#define _WIN32_WINNT_WIN10                  0x0A00 /* ABRACADABRA_THRESHOLD * /
*/

	if (NULL == PPG_si_osType) {
		char *defaultTypeName = "Windows";
#if !defined(_WIN32_WINNT_WINBLUE) || !(_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE)
		OSVERSIONINFOEX versionInfo;
#endif
		PPG_si_osType = defaultTypeName; /* by default, use the "unrecognized version" string */
		PPG_si_osTypeOnHeap = NULL;

#if defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE)
		/* OS Versions:  https://msdn.microsoft.com/en-us/library/windows/desktop/ms724832(v=vs.85).aspx */
		if (IsWindowsServer()) {
#if defined(_WIN32_WINNT_WIN10) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WIN10)
			if (IsWindows10OrGreater()) {
				PPG_si_osType = "Windows Server 2016";
			} else
#endif /* defined(_WIN32_WINNT_WIN10) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WIN10) */
			if (IsWindows8Point1OrGreater()) {
				PPG_si_osType = "Windows Server 2012 R2";
			} else if (IsWindows8OrGreater()) {
				PPG_si_osType = "Windows Server 2012";
			} else if (IsWindows7OrGreater()) {
				PPG_si_osType = "Windows Server 2008 R2";
			} else if (IsWindowsVistaOrGreater()) {
				PPG_si_osType = "Windows Server 2008";
			} else if (IsWindowsXPOrGreater()) {
				PPG_si_osType = "Windows Server 2003";
			}
		} else {
#if defined(_WIN32_WINNT_WIN10) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WIN10)
			if (IsWindows10OrGreater()) {
				PPG_si_osType = "Windows 10";
			} else
#endif /* defined(_WIN32_WINNT_WIN10) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WIN10) */
			if (IsWindows8Point1OrGreater()) {
				PPG_si_osType = "Windows 8.1";
			} else if (IsWindows8OrGreater()) {
				PPG_si_osType = "Windows 8";
			} else if (IsWindows7OrGreater()) {
				PPG_si_osType = "Windows 7";
			} else if (IsWindowsVistaOrGreater()) {
				PPG_si_osType = "Windows Vista";
			} else if (IsWindowsXPOrGreater()) {
				PPG_si_osType = "Windows XP";
			}
		}
#else /* defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE) */
		versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
		if (!GetVersionEx((OSVERSIONINFO *) &versionInfo)) {
			return NULL;
		}

		if (VER_PLATFORM_WIN32_NT == versionInfo.dwPlatformId) {
			/*
			 * Java 8 and later support Windows 7 and later.
			 * Include legacy versions out of sympathy.
			 */
			switch (versionInfo.dwMajorVersion) {
			case 5: {
				switch (versionInfo.dwMinorVersion) {
				case 0:
					PPG_si_osType = "Windows 2000";
					break;
				case 2:
					switch (versionInfo.wProductType) {
					case VER_NT_WORKSTATION:
						PPG_si_osType = "Windows XP";
						break;
					case VER_NT_DOMAIN_CONTROLLER: /* FALLTHROUGH */
					case VER_NT_SERVER:
					default :
						PPG_si_osType = "Windows Server 2003";
						break;
					}
					break;
				default:
					PPG_si_osType = defaultTypeName;
					break;
				}
				break;
			}
			case 6: {
				switch (versionInfo.wProductType) {
				case VER_NT_WORKSTATION: {
					switch (versionInfo.dwMinorVersion) {
					case 0:
						PPG_si_osType = "Windows Vista";
						break;
					case 1:
						PPG_si_osType = "Windows 7";
						break;
					case 2:
						PPG_si_osType = "Windows 8";
						break;
					case 3:
						PPG_si_osType = "Windows 8.1";
						break;
					default:
						PPG_si_osType = defaultTypeName;
						break;
					} /* VER_NT_WORKSTATION */
					break;
				}
				default: {
					switch (versionInfo.dwMinorVersion) {
					case 0:
						PPG_si_osType = "Windows Server 2008";
						break;
					case 1:
						PPG_si_osType = "Windows Server 2008 R2";
						break;
					case 2:
						PPG_si_osType = "Windows Server 2012";
						break;
					case 3:
						PPG_si_osType = "Windows Server 2012 R2";
						break;
					default:
						PPG_si_osType = defaultTypeName;
						break;
					}
					break;
				}

				} /* switch (versionInfo.wProductType) */
				/* (versionInfo.dwMajorVersion == 6) */
				break;
			}
			case 10: {
				switch (versionInfo.wProductType) {
				case VER_NT_WORKSTATION: {
					switch (versionInfo.dwMinorVersion) {
					case 0:
						PPG_si_osType = "Windows 10";
						break;
					default:
						PPG_si_osType = defaultTypeName;
						break;
					}
				}
				break;
				default: {
					switch (versionInfo.dwMinorVersion) {
					case 0:
						PPG_si_osType = "Windows Server 2016";
						break;
					default:
						PPG_si_osType = defaultTypeName;
						break;
					}
				}
				break;
				}
			}
			break;
			default:
				PPG_si_osType = defaultTypeName;
				break;
			}

		}
#endif /* defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE) */

		if (defaultTypeName == PPG_si_osType) {
			HKEY hKey;
			if (ERROR_SUCCESS == RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey)) {
				DWORD nameSize = 0;
				/* query the first time to get the content size of the value. */
				if (ERROR_SUCCESS == RegQueryValueExA(hKey, "ProductName", NULL, NULL, NULL, &nameSize)) {
					char *productNameBuffer = portLibrary->mem_allocate_memory(portLibrary, nameSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
					if (NULL != productNameBuffer) {
						/* query the second time to get the value */
						if (ERROR_SUCCESS == RegQueryValueExA(hKey, "ProductName", NULL, NULL, (LPBYTE)productNameBuffer, &nameSize)) {
							PPG_si_osType = PPG_si_osTypeOnHeap = productNameBuffer;
						} else {
							portLibrary->mem_free_memory(portLibrary, productNameBuffer);
						}
					}
				}
			}
		}
		if (defaultTypeName == PPG_si_osType) {
#if defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE)
			Trc_PRT_sysinfo_failed_to_get_os_type();
#else
			Trc_PRT_sysinfo_unrecognized_Windows_version(versionInfo.wProductType, versionInfo.dwMinorVersion, versionInfo.dwMajorVersion);
#endif
		}
	}
	return PPG_si_osType;
}
/**
 * Determine version information from the operating system.
 *
 * @param[in] portLibrary The port library.
 *
 * @return OS version string (NULL terminated) on success, NULL on error.
 *
 * @note portLibrary is responsible for allocation/deallocation of returned buffer.
 */
const char *
omrsysinfo_get_OS_version(struct OMRPortLibrary *portLibrary)
{
	/* OS Version format 	: <CurrentVersion> build <CurrentBuildNumber> <CSDVersion>
	 * Sample				: 6.1 build 7601 Service Pack 1
	 * */
	if (NULL == PPG_si_osVersion) {
#if defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE)
#if defined(_WIN32_WINNT_WIN10) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WIN10)
			if (IsWindows10OrGreater()) {
				PPG_si_osVersion = "10.0 build 10240";
			} else
#endif /* defined(_WIN32_WINNT_WIN10) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WIN10) */
			if (IsWindows8Point1OrGreater()) {
				PPG_si_osVersion = "6.3 build 9600";
			} else if (IsWindows8OrGreater()) {
				PPG_si_osVersion = "6.2 build 9200";
			} else if (IsWindows7SP1OrGreater()) {
				PPG_si_osVersion = "6.1 build 7601 Sevice Pack 1";
			} else if (IsWindows7OrGreater()) {
				PPG_si_osVersion = "6.1 build 7600";
			} else if (IsWindowsVistaSP2OrGreater()) {
				PPG_si_osVersion = "6.0 build 6002 Sevice Pack 2";
			} else if (IsWindowsVistaSP1OrGreater()) {
				PPG_si_osVersion = "6.0 build 6001 Sevice Pack 1";
			} else if (IsWindowsVistaOrGreater()) {
				PPG_si_osVersion = "6.0 build 6000";
/*  The Windows XP Service Packs do not update the version number. */
			} else if (IsWindowsXPSP3OrGreater()) {
				PPG_si_osVersion = "5.1 build 2600 Sevice Pack 3";
			} else if (IsWindowsXPSP2OrGreater()) {
				PPG_si_osVersion = "5.1 build 2600 Sevice Pack 2";
			} else if (IsWindowsXPSP1OrGreater()) {
				PPG_si_osVersion = "5.1 build 2600 Sevice Pack 1";
			} else if (IsWindowsXPOrGreater()) {
				PPG_si_osVersion = "5.1 build 2600";
			}
#else /* defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE) */
		OSVERSIONINFOW versionInfo;
		int len = sizeof("0123456789.0123456789 build 0123456789 ") + 1;
		char *buffer;
		uintptr_t position;

		versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
		if (!GetVersionExW(&versionInfo)) {
			return NULL;
		}

		if (NULL != versionInfo.szCSDVersion) {
			len += WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, versionInfo.szCSDVersion, -1, NULL, 0, NULL, NULL);
		}
		buffer = portLibrary->mem_allocate_memory(portLibrary, len, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == buffer) {
			return NULL;
		}

		position = portLibrary->str_printf(portLibrary, buffer, len, "%d.%d build %d",
										   versionInfo.dwMajorVersion,
										   versionInfo.dwMinorVersion,
										   versionInfo.dwBuildNumber & 0x0000FFFF);

		if ((NULL != versionInfo.szCSDVersion) && ('\0' != versionInfo.szCSDVersion[0])) {
			buffer[position++] = ' ';
			WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, versionInfo.szCSDVersion, -1, &buffer[position], (int)(len - position - 1), NULL, NULL);
		}
		PPG_si_osVersion = buffer;
#endif /* defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE) */
	}
	return PPG_si_osVersion;
}

intptr_t
omrsysinfo_process_exists(struct OMRPortLibrary *portLibrary, uintptr_t pid)
{
	HANDLE hProcess;
	intptr_t rc = 0;

	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
	/* OpenProcess returns NULL if the process does not exist or if the call fails for other reasons */

	if (NULL == hProcess) {
		DWORD lastError = GetLastError();
		/*
		 * if we are querying another user's process, we get ERROR_ACCESS_DENIED.
		 * For dead processes we get ERROR_INVALID_PARAMETER
		 */
		if (ERROR_ACCESS_DENIED == lastError) {
			rc = 1;
		}
	} else {
		DWORD exitCode;
		uintptr_t callSucceeded;

		callSucceeded = GetExitCodeProcess(hProcess, (LPDWORD)&exitCode);
		if ((0 != callSucceeded) && (STILL_ACTIVE == exitCode)) {
			rc = 1;
		}
		CloseHandle(hProcess);
	}
	return rc;
}

/**
 * Determine the process ID of the calling process.
 *
 * @param[in] portLibrary The port library.
 *
 * @return the PID.
 */
uintptr_t
omrsysinfo_get_pid(struct OMRPortLibrary *portLibrary)
{
	return GetCurrentProcessId();
}

uintptr_t
omrsysinfo_get_ppid(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

uintptr_t
omrsysinfo_get_euid(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

uintptr_t
omrsysinfo_get_egid(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

intptr_t
omrsysinfo_get_groups(struct OMRPortLibrary *portLibrary, uint32_t **gidList, uint32_t categoryCode)
{
	return -1;
}

/**
 * @internal Helper routine that determines the full path of the current executable
 * that launched the JVM instance.
 */
static intptr_t
find_executable_name(struct OMRPortLibrary *portLibrary, char **result)
{
	DWORD length;
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE];
	char *utf8Result;

	length = GetModuleFileNameW(NULL, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (!length || (length >= UNICODE_BUFFER_SIZE)) {
		return -1;
	}
	unicodeBuffer[length] = '\0';

	utf8Result = portLibrary->mem_allocate_memory(portLibrary, (length + 1) * 3, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == utf8Result) {
		return -1;
	}
	port_convertToUTF8(portLibrary, unicodeBuffer, utf8Result, length * 3);
	*result = utf8Result;
	return 0;
}

/**
 * Determines an absolute pathname for the executable.
 *
 * @param[in] portLibrary The port library.
 * @param[in] argv0 argv[0] value
 * @param[out] result Null terminated pathname string
 *
 * @return 0 on success, -1 on error (or information is not available).
 *
 * @note Caller should /not/ de-allocate memory in the result buffer, as string containing
 * the executable name is system-owned (managed internally by the port library).
 */
intptr_t
omrsysinfo_get_executable_name(struct OMRPortLibrary *portLibrary, const char *argv0, char **result)
{
	(void) argv0; /* @args used */

	/* Clear any pending error conditions. */
	portLibrary->error_set_last_error(portLibrary, 0, 0);

	if (PPG_si_executableName) {
		*result = PPG_si_executableName;
		return 0;
	}
	*result = NULL;
	return (intptr_t)-1;
}

uintptr_t
omrsysinfo_get_number_CPUs_by_type(struct OMRPortLibrary *portLibrary, uintptr_t type)
{
	uintptr_t toReturn = 0;

	Trc_PRT_sysinfo_get_number_CPUs_by_type_Entered();

	switch (type) {
	case OMRPORT_CPU_PHYSICAL:
	case OMRPORT_CPU_ONLINE: {
		SYSTEM_INFO aSysInfo;
		GetSystemInfo(&aSysInfo);
		toReturn = aSysInfo.dwNumberOfProcessors;

		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedPhysical("(no errno) ", 0);
		}

		break;
	}
	case OMRPORT_CPU_BOUND: {
		uintptr_t processAffinity = 0;
		uintptr_t systemAffinity = 0;
		HANDLE currentProcess = GetCurrentProcess();
		uintptr_t count = 0;
		int32_t i = 0;
		uintptr_t mask = 0x1;

		GetProcessAffinityMask(currentProcess, (PDWORD_PTR) &processAffinity, (PDWORD_PTR) &systemAffinity);
		/*
		 * Count the number of bound CPU's
		 */
		for (i = 0; i < (sizeof(DWORD64) * 8); i++) {
			count += (0 == (processAffinity & mask)) ? 0 : 1;
			mask <<= 1;
		}

		toReturn = count;

		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedBound("errno: ", GetLastError());
		}

		break;
	}
	case OMRPORT_CPU_ENTITLED:
		toReturn = portLibrary->portGlobals->entitledCPUs;
		break;
	case OMRPORT_CPU_TARGET: {
		uintptr_t entitled = portLibrary->portGlobals->entitledCPUs;
		uintptr_t bound = omrsysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_BOUND);
		if (entitled != 0 && entitled < bound) {
			toReturn = entitled;
		} else {
			toReturn = bound;
		}
		break;
	}
	default:
		/* Invalid argument */
		toReturn = 0;
		Trc_PRT_sysinfo_get_number_CPUs_by_type_invalidType();
		break;
	}

	Trc_PRT_sysinfo_get_number_CPUs_by_type_Exit(type, toReturn);

	return toReturn;
}

/* Paths for pdh memory counters. */
#define MEMORY_COMMIT_LIMIT_COUNTER_PATH        "\\Memory\\Commit Limit"
#define MEMORY_COMMITTED_BYTES_COUNTER_PATH     "\\Memory\\Committed Bytes"
#define MEMORY_CACHE_BYTES_COUNTER_PATH         "\\Memory\\Cache Bytes"

int32_t
omrsysinfo_get_memory_info(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo, ...)
{
	int32_t rc = -1;

	MEMORYSTATUSEX aMemStatusEx = {0};

	/* Handles for pdh memory counters. */
	PDH_HCOUNTER memoryCommitLimitCounter = NULL;
	PDH_HCOUNTER memoryCommittedBytesCounter = NULL;
	PDH_HCOUNTER memoryCacheBytesCounter = NULL;
	/* Handle for querying pdh performance data. */
	PDH_HQUERY statsHandle = NULL;
	PDH_STATUS status = ERROR_SUCCESS;
	PDH_RAW_COUNTER counterValue = {PDH_CSTATUS_INVALID_DATA, {0, 0}, 0, 0, 0};

	Trc_PRT_sysinfo_get_memory_info_Entered();

	if (NULL == memInfo) {
		Trc_PRT_sysinfo_get_memory_info_Exit(OMRPORT_ERROR_SYSINFO_NULL_OBJECT_RECEIVED);
		return OMRPORT_ERROR_SYSINFO_NULL_OBJECT_RECEIVED;
	}

	memInfo->totalPhysical = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->availPhysical = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->totalVirtual = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->availVirtual = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->totalSwap = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->availSwap = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->cached = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->buffered = OMRPORT_MEMINFO_NOT_AVAILABLE;

	aMemStatusEx.dwLength = sizeof(aMemStatusEx);
	rc = GlobalMemoryStatusEx(&aMemStatusEx);
	/* Win32 API GlobalMemoryStatusEx() returns 0 on failure. */
	if (0 == rc) {
		Trc_PRT_sysinfo_get_memory_info_memStatFailed(GetLastError());
		Trc_PRT_sysinfo_get_memory_info_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
	}

	memInfo->totalPhysical = (uint64_t)aMemStatusEx.ullTotalPhys;
	memInfo->availPhysical = (uint64_t)aMemStatusEx.ullAvailPhys;
	memInfo->totalVirtual = (uint64_t)aMemStatusEx.ullTotalVirtual;
	memInfo->availVirtual = (uint64_t)aMemStatusEx.ullAvailVirtual;

	/* Create pdh handle for managing the performance data collection. */
	status = PdhOpenQuery(NULL, (DWORD_PTR)NULL, (PDH_HQUERY *)&statsHandle);
	if (ERROR_SUCCESS != status) {
		Trc_PRT_sysinfo_get_memory_info_pdhOpenQueryFailed(status);
		Trc_PRT_sysinfo_get_memory_info_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
	}

	status = PdhAddCounter(statsHandle,
						   MEMORY_COMMIT_LIMIT_COUNTER_PATH,
						   (DWORD_PTR)NULL,
						   &memoryCommitLimitCounter);
	if (ERROR_SUCCESS != status) {
		Trc_PRT_sysinfo_get_memory_info_failedAddingCounter("Commit Limit", status);
		Trc_PRT_sysinfo_get_memory_info_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO);
		PdhCloseQuery(statsHandle);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
	}

	status = PdhAddCounter(statsHandle,
						   MEMORY_COMMITTED_BYTES_COUNTER_PATH,
						   (DWORD_PTR)NULL,
						   &memoryCommittedBytesCounter);
	if (ERROR_SUCCESS != status) {
		Trc_PRT_sysinfo_get_memory_info_failedAddingCounter("Committed Bytes", status);
		Trc_PRT_sysinfo_get_memory_info_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO);
		PdhCloseQuery(statsHandle);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
	}

	status = PdhAddCounter(statsHandle,
						   MEMORY_CACHE_BYTES_COUNTER_PATH,
						   (DWORD_PTR)NULL,
						   &memoryCacheBytesCounter);
	if (ERROR_SUCCESS != status) {
		Trc_PRT_sysinfo_get_memory_info_failedAddingCounter("Cache Bytes", status);
		Trc_PRT_sysinfo_get_memory_info_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO);
		PdhCloseQuery(statsHandle);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
	}

	/* Collect the current raw data value for all counters in the usage stats query. */
	status = PdhCollectQueryData(statsHandle);
	if (ERROR_SUCCESS != status) {
		Trc_PRT_sysinfo_get_memory_info_dataQueryFailed();
		Trc_PRT_sysinfo_get_memory_info_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO);
		PdhCloseQuery(statsHandle);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
	}

	status = PdhGetRawCounterValue(memoryCommitLimitCounter, (LPDWORD)NULL, &counterValue);
	if ((ERROR_SUCCESS == status) && ((PDH_CSTATUS_VALID_DATA == counterValue.CStatus) ||
									  (PDH_CSTATUS_NEW_DATA == counterValue.CStatus))) {
		memInfo->totalSwap = counterValue.FirstValue;
	}

	status = PdhGetRawCounterValue(memoryCommittedBytesCounter, (LPDWORD)NULL, &counterValue);
	if ((ERROR_SUCCESS == status) && ((PDH_CSTATUS_VALID_DATA == counterValue.CStatus) ||
									  (PDH_CSTATUS_NEW_DATA == counterValue.CStatus))) {
		memInfo->availSwap = (memInfo->totalSwap - counterValue.FirstValue);
	}

	status = PdhGetRawCounterValue(memoryCacheBytesCounter, (LPDWORD)NULL, &counterValue);
	if ((ERROR_SUCCESS == status) && ((PDH_CSTATUS_VALID_DATA == counterValue.CStatus) ||
									  (PDH_CSTATUS_NEW_DATA == counterValue.CStatus))) {
		memInfo->cached = counterValue.FirstValue;
	}
	/* Note that Windows does not have 'buffered memory' and hence, memInfo->buffered remains -1. */

	memInfo->timestamp = (portLibrary->time_nano_time(portLibrary) / NANOSECS_PER_USEC);

	Trc_PRT_sysinfo_get_memory_info_Exit(0);
	PdhCloseQuery(statsHandle);
	return 0;
}

/**
 * Determine the size of the total physical memory in the system, in bytes.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 if the information was unavailable, otherwise total physical memory in bytes.
 */
uint64_t
omrsysinfo_get_physical_memory(struct OMRPortLibrary *portLibrary)
{
	MEMORYSTATUSEX aMemStatusEx;

	aMemStatusEx.dwLength = sizeof(aMemStatusEx);
	if (!GlobalMemoryStatusEx(&aMemStatusEx)) {
		return J9CONST64(0);
	}

	return (uint64_t) aMemStatusEx.ullTotalPhys;
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrsysinfo_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library.
 *
 * @note Most implementations will be empty.
 */
void
omrsysinfo_shutdown(struct OMRPortLibrary *portLibrary)
{
	if (NULL != portLibrary->portGlobals) {
#if defined(_WIN32_WINNT_WINBLUE) && (_WIN32_WINNT_MAXVER >= _WIN32_WINNT_WINBLUE)
		PPG_si_osVersion = NULL;
#else
		if (PPG_si_osVersion) {
			portLibrary->mem_free_memory(portLibrary, PPG_si_osVersion);
			PPG_si_osVersion = NULL;
		}
#endif

		if (NULL != PPG_si_osTypeOnHeap) {
			portLibrary->mem_free_memory(portLibrary, PPG_si_osTypeOnHeap);
			PPG_si_osTypeOnHeap = NULL;
		}

		/* Two conditions for PPG_si_osType, no need to free for both case:
		 * 1. pointing to same memory as PPG_si_osTypeOnHeap
		 * 2. pointing to static string
		 */
		PPG_si_osType = NULL;

		if (NULL != PPG_si_executableName) {
			portLibrary->mem_free_memory(portLibrary, PPG_si_executableName);
			PPG_si_executableName = NULL;
		}
	}
}
/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the system information operations may be created here.  All resources created here should be destroyed
 * in @ref omrsysinfo_shutdown.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_SYSINFO
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrsysinfo_startup(struct OMRPortLibrary *portLibrary)
{
	/* Obtain and cache executable name; if this fails, executable name remains NULL, but
	 * shouldn't cause failure to startup port library.  Failure will be noticed only
	 * when the omrsysinfo_get_executable_name() actually gets invoked.
	 */
	(void) find_executable_name(portLibrary, &PPG_si_executableName);
	return 0;
}

/**
 * Query the operating system for the name of the user associate with the current thread
 *
 * Obtain the value of the name of the user associated with the current thread, and then write it out into the buffer
* supplied by the user
*
* @param[in] portLibrary The port Library
* @param[out] buffer Buffer for the name of the user
* @param[in,out] length The length of the buffer
*
* @return 0 on success, number of bytes required to hold the
* information if the output buffer was too small, -1 on failure.
*
* @note buffer is undefined on error or when supplied buffer was too small.
*/
intptr_t
omrsysinfo_get_username(struct OMRPortLibrary *portLibrary, char *buffer, uintptr_t length)
{
	DWORD rc, nameLength;
	wchar_t *unicodeBuffer;
	intptr_t result;

	nameLength = (DWORD)length;
	unicodeBuffer = portLibrary->mem_allocate_memory(portLibrary, nameLength * 2, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == unicodeBuffer) {
		return -1;
	}
	rc = GetUserNameW(unicodeBuffer, &nameLength);

	/* If the function succeeds, the return value is the number of characters stored into
	the buffer, including the terminating null character. If the buffer is not large enough,
	call fails and the required size is stored in nameLength.
	*/

	if (0 == rc) {
		if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
			/* overestimate */
			result = nameLength * 3;
		} else {
			result = -1;
		}
	} else {
		/* convert */
		rc = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeBuffer, -1, NULL, 0, NULL, NULL);
		if (0 == rc) {
			result = -1;
		} else {
			if (rc > length) {
				/* buffer is too small */
				result = rc;
			} else {
				WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeBuffer, -1,  buffer, (int)length, NULL, NULL);
				result = 0;
			}
		}
	}
	portLibrary->mem_free_memory(portLibrary, unicodeBuffer);
	return result;
}
/**
 * Query the operating system for the name of the group associate with the current thread
 *
 * Obtain the value of the name of the group associated with the current thread, and then write it out into the buffer
* supplied by the user
*
* @param[in] portLibrary The port Library
* @param[out] buffer Buffer for the name of the group
* @param[in,out] length The length of the buffer
*
* @return 0 on success, number of bytes required to hold the
* information if the output buffer was too small, -1 on failure.
*
* @note buffer is undefined on error or when supplied buffer was too small.
*/
intptr_t
omrsysinfo_get_groupname(struct OMRPortLibrary *portLibrary, char *buffer, uintptr_t length)
{
	/* Not applicable to Windows */
	return -1;
}

uint32_t
omrsysinfo_get_limit(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t *limit)
{
	Trc_PRT_sysinfo_get_limit_Entered(resourceID);
	*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
	Trc_PRT_sysinfo_get_limit_Exit(OMRPORT_LIMIT_UNKNOWN);
	return OMRPORT_LIMIT_UNKNOWN;
}

uint32_t
omrsysinfo_set_limit(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t limit)
{
	Trc_PRT_sysinfo_set_limit_Entered(resourceID, limit);
	Trc_PRT_sysinfo_set_limit_Exit(-1);
	return -1;
}

/**
 * Provides the system load average for the past 1, 5 and 15 minutes, if available.
 * The load average is defined as the number of runnable (including running) processes
 *  averaged over a period of time, however, it may also include processes in uninterruptable
 *  sleep states (Linux does this).
 *
 * @param[in] portLibrary The port library.
 * @param[out] loadAverage must be non-Null. Contains the load average data for the past 1, 5 and 15 minutes.
 * 			A load average of -1 indicates the load average for that specific period was not available.
 *
 * @return 0 on success, non-zero on error.
 *
 */
intptr_t
omrsysinfo_get_load_average(struct OMRPortLibrary *portLibrary, struct J9PortSysInfoLoadData *loadAverageData)
{
	return -1;
}

intptr_t
omrsysinfo_get_CPU_utilization(struct OMRPortLibrary *portLibrary, struct J9SysinfoCPUTime *cpuTime)
{

	FILETIME lpIdleTime;
	FILETIME lpKernelTime;
	FILETIME lpUserTime;
	uint64_t idleTime = 0;
	uint64_t kernelActiveTime = 0;
	uint64_t userTime = 0;

	/*
	 * GetSystemTimes returns the sum of CPU times across all CPUs
	 */

	if (0 == GetSystemTimes(&lpIdleTime, &lpKernelTime, &lpUserTime)) {
		Trc_PRT_sysinfo_get_CPU_utilization_GetSystemTimesFailed(GetLastError());
		return OMRPORT_ERROR_SYSINFO_GET_STATS_FAILED;
	}
	idleTime = (lpIdleTime.dwLowDateTime + (((uint64_t) lpIdleTime.dwHighDateTime) << 32)) * 100; /* FILETIME resolution is 100 ns */
	kernelActiveTime = (lpKernelTime.dwLowDateTime + (((uint64_t) lpKernelTime.dwHighDateTime) << 32)) * 100
					   - idleTime; /* kernel time includes idle time */
	Trc_PRT_sysinfo_get_CPU_utilization_GST_result("kernel active", kernelActiveTime);
	userTime = (lpUserTime.dwLowDateTime + (((uint64_t) lpUserTime.dwHighDateTime) << 32)) * 100;
	Trc_PRT_sysinfo_get_CPU_utilization_GST_result("user", userTime);
	cpuTime->cpuTime = kernelActiveTime + userTime;
	cpuTime->timestamp = portLibrary->time_nano_time(portLibrary);
	if (0 == cpuTime->timestamp) {
		Trc_PRT_sysinfo_get_CPU_utilization_timeFailed();
		return OMRPORT_ERROR_SYSINFO_INVALID_TIME;
	}
	cpuTime->numberOfCpus = (int32_t) portLibrary->sysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_ONLINE);

	return 0;
}

int32_t
omrsysinfo_limit_iterator_init(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state)
{
	return OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
}

BOOLEAN
omrsysinfo_limit_iterator_hasNext(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state)
{
	return FALSE;
}

int32_t
omrsysinfo_limit_iterator_next(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state, J9SysinfoUserLimitElement *limitElement)
{
	return OMRPORT_ERROR_SYSINFO_OPFAILED;
}

static int32_t
copyEnvToBuffer(struct OMRPortLibrary *portLibrary, void *args)
{
	CopyEnvToBufferArgs *copyEnvToBufferArgs = (CopyEnvToBufferArgs *) args;
	uint8_t *buffer = copyEnvToBufferArgs->buffer;
	uintptr_t bufferSize = copyEnvToBufferArgs->bufferSizeBytes;
	BOOLEAN bufferBigEnough = TRUE;
	BOOLEAN firstEnvVar;
	int storageRequired;
	uintptr_t spaceLeft;
	wchar_t *envVarsPtrW;
	wchar_t *envVars;
	uint8_t *cursor = NULL;
	int32_t rc;

	/* grab the environment variables */
	envVarsPtrW = GetEnvironmentStringsW();
	envVars = envVarsPtrW;

	if (NULL == envVars) {
		/* failed to get the environment */
		return OMRPORT_ERROR_SYSINFO_OPFAILED;
	}

	/* How much space do we need to store the environment variables? */
	storageRequired = 0;
	firstEnvVar = TRUE;
	while ((0 == envVars[0] && 0 == envVars[1]) == FALSE) {
		if (firstEnvVar) {
			firstEnvVar = FALSE;
		} else {
			envVars++; /* skip over single terminating null after each individual envVar */
		}
		storageRequired += WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, envVars, -1, (LPSTR)cursor, 0, NULL, NULL);
		envVars += wcslen(envVars);
	}

	/* allow space for \0\0 terminating characters */
	storageRequired += 2;

	/* Is the supplied buffer big enough? */
	if ((uintptr_t)storageRequired > bufferSize) {
		bufferBigEnough = FALSE;
		if (NULL == buffer) {
			/* no buffer to copy data to, so return the size requirement */
			FreeEnvironmentStringsW(envVarsPtrW);
			return storageRequired;
		}
	}

	/* Copy each entry from environment to the user supplied buffer until we run out of space */
	cursor = buffer;
	spaceLeft = bufferSize;
	copyEnvToBufferArgs->numElements = 0;

	/* reset envVars to the start of the environment block */
	envVars = envVarsPtrW;

	/* copy the environment variables into the user buffer */
	firstEnvVar = TRUE;
	while ((0 == envVars[0] && 0 == envVars[1]) == FALSE) {
		int spaceForThisEntry;
		if (firstEnvVar) {
			firstEnvVar = FALSE;
		} else {
			envVars++; /* skip over single terminating null after each individual envVar */
		}
		spaceForThisEntry = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, envVars, -1, (LPSTR)cursor, 0, NULL, NULL);
		if (spaceLeft >= (uintptr_t)spaceForThisEntry) {
			WideCharToMultiByte(OS_ENCODING_CODE_PAGE, 0, envVars, -1, (LPSTR)cursor, spaceForThisEntry, NULL, NULL);
			copyEnvToBufferArgs->numElements = copyEnvToBufferArgs->numElements + 1;
			envVars += wcslen(envVars);
			cursor += spaceForThisEntry;
			spaceLeft -= spaceForThisEntry;
		} else {
			break;
		}
	}

	/* there should be space for terminating nulls */
	if (spaceLeft > 1) {
		/* add the final terminator characters */
		*cursor++ = '\0';
		*cursor = '\0';
	}

	if (bufferBigEnough) {
		/* buffer was sufficient, so everything should be copied */
		rc = 0;
	} else {
		/* if the buffer is too small, return the size needed. */
		/* this can happen if the environment expands between calls. */
		rc = storageRequired;
	}

	if (NULL != envVarsPtrW) {
		FreeEnvironmentStringsW(envVarsPtrW);
	}
	return rc;
}

int32_t
omrsysinfo_env_iterator_init(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state, void *buffer, uintptr_t bufferSizeBytes)
{
	int32_t rc;
	CopyEnvToBufferArgs copyEnvToBufferArgs;

	copyEnvToBufferArgs.buffer = buffer;
	copyEnvToBufferArgs.bufferSizeBytes = bufferSizeBytes;
	copyEnvToBufferArgs.numElements = 0; /* this value is returned by copyEnvToBuffer */

	rc = copyEnvToBuffer(portLibrary, &copyEnvToBufferArgs);
	if (0 == rc) {
		/* successful call, return buffer information. */
		state->buffer = buffer;
		state->bufferSizeBytes = bufferSizeBytes;

		if (0 == copyEnvToBufferArgs.numElements) {
			/* This is used by omrsysinfo_env_iterator_hasNext to indicate that there are no elements */
			state->current = NULL;
		} else {
			state->current = state->buffer;
		}
	} else {
		/* successful call, clear the state information. */
		state->buffer = NULL;
		state->bufferSizeBytes = 0;
		state->current = NULL;
	}

	return rc;
}

BOOLEAN
omrsysinfo_env_iterator_hasNext(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state)
{
	if (NULL == state->current) {
		/* state->current will only be NULL if the the iterator has no elements */
		return FALSE;
	}

	return (*(char *)state->current != '\0');
}

int32_t
omrsysinfo_env_iterator_next(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state, J9SysinfoEnvElement *envElement)
{
	if (NULL == state->current) {
		return OMRPORT_ERROR_SYSINFO_OPFAILED;
	}

	envElement->nameAndValue = state->current;
	state->current = (char *)state->current + strlen(state->current) + 1;

	return 0;
}

/* PDH processor object and associated counter names. */
#define PROCESSOR_OBJECT_NAME                   "Processor"

#define PROCESSOR_USER_TIME_COUNTER_NAME        "% User Time"
#define PROCESSOR_PRIVILEGED_TIME_COUNTER_NAME  "% Privileged Time"
#define PROCESSOR_IDLE_TIME_COUNTER_NAME        "% Idle Time"

/* As of now we are dealing with only the above four counters. Update this when more counters
 * are added.
 */
#define PROCESSOR_COUNTER_CATEGORIES	3
#define SIZEOF_PDH_HCOUNTER				sizeof(PDH_HCOUNTER)
#define NS100_TO_USEC					10

/**
 * Add all instances of a pdh processor counter to the performance query.
 * @param [in] portLibrary      - pointer to port library
 * @param [in] usageStats       - a handle to the base usage statistics object.
 * @param [in] procInfo         - a handle to the processor usage statistics object.
 * @param [in] cntrName         - pdh processor counter name.
 * @param [in] cntrPtrRef       - address of processor counter reference.
 *
 * @return 0 on success and -1 on failure.
 */
static int32_t
AddCounter(OMRPortLibrary *portLibrary,
		   PDH_HQUERY statsHandle,
		   struct J9ProcessorInfos *procInfo,
		   const char *cntrName,
		   PDH_HCOUNTER **cntrPtrRef)
{
	register int32_t cpuCntr;
	PDH_HCOUNTER *cntrPtr = *cntrPtrRef;
	PDH_STATUS status = ERROR_SUCCESS;
	TCHAR counterPathBuf[BUFSIZ];

	for (cpuCntr = 0; cpuCntr < procInfo->totalProcessorCount + 1; cpuCntr++) {
		if (0 == cpuCntr) {
			portLibrary->str_printf(portLibrary, counterPathBuf, sizeof(counterPathBuf), "\\%s(%s)\\%s", PROCESSOR_OBJECT_NAME, "_Total", cntrName);
		} else {
			portLibrary->str_printf(portLibrary, counterPathBuf, sizeof(counterPathBuf), "\\%s(%d)\\%s",
									PROCESSOR_OBJECT_NAME,
									procInfo->procInfoArray[cpuCntr].proc_id,
									cntrName);
		}
		status = PdhAddCounter(statsHandle, counterPathBuf, (DWORD_PTR)NULL, &(cntrPtr[cpuCntr]));
		if (ERROR_SUCCESS != status) {
			return -1;
		}
	}
	return 0;
}

int32_t
omrsysinfo_get_processor_info(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfo)
{
	register int32_t cpuCntr = 0;
	int32_t rc = 0;

	/* Windows specific counter handles; number of handles varies depending on available processors. */
	PDH_HQUERY statsHandle = NULL;
	PDH_HCOUNTER *userTimeCounter = NULL;
	PDH_HCOUNTER *privilegedTimeCounter = NULL;
	PDH_HCOUNTER *idleTimeCounter = NULL;
	PDH_HCOUNTER *rootCntrPtr = NULL;
	PDH_RAW_COUNTER counterValue = {PDH_CSTATUS_INVALID_DATA, {0, 0}, 0, 0, 0};
	PDH_STATUS status = ERROR_SUCCESS;

	Trc_PRT_sysinfo_get_processor_info_Entered();

	if (NULL == procInfo) {
		Trc_PRT_sysinfo_get_processor_info_Exit(OMRPORT_ERROR_SYSINFO_NULL_OBJECT_RECEIVED);
		return OMRPORT_ERROR_SYSINFO_NULL_OBJECT_RECEIVED;
	}

	/* Keep looping until we get the size of processor set right, i.e., what if it changes between our
	 * allocating space and actually obtaining the processor data? We shall need to rehash things here.
	 */
	do {
		uintptr_t procInfoArraySize = 0;
		uintptr_t rootCntrPtrSize = 0;

		/* Obtain the number of processors on the machine. */
		procInfo->totalProcessorCount = (int32_t)portLibrary->sysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_PHYSICAL);
		Assert_PRT_true(0 < procInfo->totalProcessorCount);

		procInfoArraySize = (procInfo->totalProcessorCount + 1) * sizeof(J9ProcessorInfo);
		procInfo->procInfoArray = portLibrary->mem_allocate_memory(portLibrary,
								  procInfoArraySize,
								  OMR_GET_CALLSITE(),
								  OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL ==  procInfo->procInfoArray) {
			Trc_PRT_sysinfo_get_processor_info_memAllocFailed();
			Trc_PRT_sysinfo_get_processor_info_Exit(OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED);
			return OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED;
		}

		/* Associate a processor id to each processor on the system. */
		for (cpuCntr = 0; cpuCntr < procInfo->totalProcessorCount + 1; cpuCntr++) {
			/* Start with the system record that is marked as -1. */
			procInfo->procInfoArray[cpuCntr].proc_id = cpuCntr - 1;
			procInfo->procInfoArray[cpuCntr].userTime = OMRPORT_PROCINFO_NOT_AVAILABLE;
			procInfo->procInfoArray[cpuCntr].systemTime = OMRPORT_PROCINFO_NOT_AVAILABLE;
			procInfo->procInfoArray[cpuCntr].idleTime = OMRPORT_PROCINFO_NOT_AVAILABLE;
			procInfo->procInfoArray[cpuCntr].waitTime = OMRPORT_PROCINFO_NOT_AVAILABLE;
			procInfo->procInfoArray[cpuCntr].busyTime = OMRPORT_PROCINFO_NOT_AVAILABLE;
			procInfo->procInfoArray[cpuCntr].online = OMRPORT_PROCINFO_PROC_OFFLINE;
		}

		/* online field for the aggregate record needs to be -1 */
		procInfo->procInfoArray[0].online = -1;

		rootCntrPtrSize = (SIZEOF_PDH_HCOUNTER * (procInfo->totalProcessorCount + 1) * PROCESSOR_COUNTER_CATEGORIES);
		/* Allocate memory for all processor related counters at once. */
		rootCntrPtr = (PDH_HCOUNTER *)portLibrary->mem_allocate_memory(portLibrary,
					  rootCntrPtrSize,
					  OMR_GET_CALLSITE(),
					  OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == rootCntrPtr) {
			Trc_PRT_sysinfo_get_processor_info_categoryAllocFailed(PROCESSOR_COUNTER_CATEGORIES,
					PROCESSOR_OBJECT_NAME);
			portLibrary->mem_free_memory(portLibrary, procInfo->procInfoArray);
			Trc_PRT_sysinfo_get_processor_info_Exit(OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED);
			return OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED;
		}
		memset(rootCntrPtr, 0, rootCntrPtrSize);

		status = PdhOpenQuery(NULL, (DWORD_PTR)NULL, (PDH_HQUERY *)&statsHandle);
		if (ERROR_SUCCESS != status) {
			Trc_PRT_sysinfo_get_processor_info_pdhOpenQueryFailed(status);
			portLibrary->mem_free_memory(portLibrary, rootCntrPtr);
			Trc_PRT_sysinfo_get_processor_info_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO);
			return OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO;
		}

		userTimeCounter = rootCntrPtr;
		privilegedTimeCounter = (userTimeCounter + (procInfo->totalProcessorCount + 1));
		idleTimeCounter = (privilegedTimeCounter + (procInfo->totalProcessorCount + 1));

		/* Add the % User Time counter. */
		rc = AddCounter(portLibrary, statsHandle, procInfo, PROCESSOR_USER_TIME_COUNTER_NAME, &userTimeCounter);
		if (0 != rc) {
			rc = OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO;
			Trc_PRT_sysinfo_get_processor_info_failedAddingCounter("User Time");
			goto _cleanup;
		}

		/* Add the % Privilege Time - or as Windows calls - privileged time counter. */
		rc =  AddCounter(portLibrary, statsHandle, procInfo, PROCESSOR_PRIVILEGED_TIME_COUNTER_NAME, &privilegedTimeCounter);
		if (0 != rc) {
			rc = OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO;
			Trc_PRT_sysinfo_get_processor_info_failedAddingCounter("System Time");
			goto _cleanup;
		}

		/* Add the % Idle Time counter. */
		rc = AddCounter(portLibrary, statsHandle, procInfo, PROCESSOR_IDLE_TIME_COUNTER_NAME, &idleTimeCounter);
		if (0 != rc) {
			rc = OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO;
			Trc_PRT_sysinfo_get_processor_info_failedAddingCounter("Idle Time");
			goto _cleanup;
		}

		/* Check whether the number of processors have changed since we last obtained this count and
		 * allocated enough memory for the same.
		 */
		if (((int32_t)portLibrary->sysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_PHYSICAL)) == procInfo->totalProcessorCount) {

			/* Nailed it! The processor count we obtained seems to hold and thus, we have sufficient space
			 * to forge ahead with collecting data. Collect current raw data value for all counters in the
			 * usage stats query.
			 */
			status = PdhCollectQueryData(statsHandle);
			if (ERROR_SUCCESS != status) {
				rc = OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO;
				Trc_PRT_sysinfo_get_processor_info_failedCollectingPdhData();
				goto _cleanup;
			}
			break;
		} /* end outer-if */

		/* Seems the processor count changed between getting the size and actually collecting data. Must
		 * free up the memory since this is insufficient for us to continue with, and start all over again.
		 */
		portLibrary->mem_free_memory(portLibrary, rootCntrPtr);
		rootCntrPtr = NULL;
		portLibrary->mem_free_memory(portLibrary, procInfo->procInfoArray);
		procInfo->procInfoArray = NULL;
	} while (1);

	/* On Windows all processors that are configured are supposed to be online as well. Windows' counters
	 * are in 100s' of nanoseconds. So convert this to microseconds.
	 */
	for (cpuCntr = 0; cpuCntr < procInfo->totalProcessorCount + 1; cpuCntr++) {
		status = PdhGetRawCounterValue(userTimeCounter[cpuCntr], (LPDWORD)NULL, &counterValue);
		if ((ERROR_SUCCESS == status) &&
			((PDH_CSTATUS_VALID_DATA == counterValue.CStatus) ||
			 (PDH_CSTATUS_NEW_DATA == counterValue.CStatus))) {
			procInfo->procInfoArray[cpuCntr].userTime = counterValue.FirstValue / NS100_TO_USEC;
		}

		status = PdhGetRawCounterValue(privilegedTimeCounter[cpuCntr], (LPDWORD)NULL, &counterValue);
		if ((ERROR_SUCCESS == status) &&
			((PDH_CSTATUS_VALID_DATA == counterValue.CStatus) ||
			 (PDH_CSTATUS_NEW_DATA == counterValue.CStatus))) {
			procInfo->procInfoArray[cpuCntr].systemTime = counterValue.FirstValue / NS100_TO_USEC;
		}

		/* On Windows we compute busy times based on system and user times, though we need add the
		 * Processor Times counter for retrieving the processor timestamp, obtained below.
		 */
		procInfo->procInfoArray[cpuCntr].busyTime = procInfo->procInfoArray[cpuCntr].systemTime +
				procInfo->procInfoArray[cpuCntr].userTime;

		status = PdhGetRawCounterValue(idleTimeCounter[cpuCntr], (LPDWORD)NULL, &counterValue);
		if ((ERROR_SUCCESS == status) &&
			((PDH_CSTATUS_VALID_DATA == counterValue.CStatus) ||
			 (PDH_CSTATUS_NEW_DATA == counterValue.CStatus))) {
			procInfo->procInfoArray[cpuCntr].idleTime = counterValue.FirstValue / NS100_TO_USEC;

			/* Get the timestamp here itself. */
			procInfo->timestamp = (((((uint64_t)counterValue.TimeStamp.dwHighDateTime) << 32)) |
								   (((uint64_t)counterValue.TimeStamp.dwLowDateTime))) / NS100_TO_USEC;
		}

		procInfo->procInfoArray[cpuCntr].online = OMRPORT_PROCINFO_PROC_ONLINE;
	} /* end for */

	/* Windows '_Total' for each mode is actually a misnomer - since they are not the aggregates,
	 * but averages over all processors. So we need to multiply with processor counts as port
	 * library clients are interested in aggregates and also, to be consistent with other platforms.
	 */
	procInfo->procInfoArray[0].userTime *= procInfo->totalProcessorCount;
	procInfo->procInfoArray[0].systemTime *= procInfo->totalProcessorCount;
	procInfo->procInfoArray[0].busyTime *= procInfo->totalProcessorCount;
	procInfo->procInfoArray[0].idleTime *= procInfo->totalProcessorCount;

_cleanup:
	portLibrary->mem_free_memory(portLibrary, rootCntrPtr);
	if (0 != rc) {
		portLibrary->mem_free_memory(portLibrary, procInfo->procInfoArray);
	}
	Trc_PRT_sysinfo_get_processor_info_Exit(rc);
	PdhCloseQuery(statsHandle);
	return rc;
}

void
omrsysinfo_destroy_processor_info(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfos)
{
	Trc_PRT_sysinfo_destroy_processor_info_Entered();

	if (NULL != procInfos->procInfoArray) {
		portLibrary->mem_free_memory(portLibrary, procInfos->procInfoArray);
		procInfos->procInfoArray = NULL;
	}

	Trc_PRT_sysinfo_destroy_processor_info_Exit();
}

intptr_t
omrsysinfo_get_cwd(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen)
{
	DWORD pathLengthInWChars = 0;
	wchar_t *unicodeBuffer = NULL; /* unicode version of CWD, size: pathLengthInWChars*sizeof(wchar_t) plus 1 for terminator and 1 for terminating slash if required */
	int32_t portConvertRC = -1;
	DWORD unicodeBufferSizeBytes = 0;

	if (NULL == buf) {
		Assert_PRT_true(0 == bufLen);
	}

	/* GetCurrentDirectoryW returns the numbers of TCHARS needed to store the string. The returned string not necessarily ends with a backslash.
	 *     - seeing as we always operate on systems that support, TCHAR is in factt wchar_t
	 *     - see "Working with Strings"  on MSDN for more information
	 */
	pathLengthInWChars = GetCurrentDirectoryW(0, NULL);

	if (0 == pathLengthInWChars) {
		Trc_PRT_sysinfo_get_cwd_failed_getcurrentdirectory(GetLastError());
		return -1;
	}

	unicodeBufferSizeBytes = (pathLengthInWChars + 1) * sizeof(wchar_t);
	/*is buflen big enough? */
	if (bufLen < unicodeBufferSizeBytes) {
		return unicodeBufferSizeBytes;
	}

	/* we need a buffer to store the unicode string, as we'll use the supplied buf for the UTF8 conversion */
	unicodeBuffer = portLibrary->mem_allocate_memory(portLibrary, unicodeBufferSizeBytes, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);

	if (NULL == unicodeBuffer) {
		Trc_PRT_sysinfo_get_cwd_oome();
		return -1;
	}

	pathLengthInWChars = GetCurrentDirectoryW(pathLengthInWChars + 1, unicodeBuffer);

	if (0 == pathLengthInWChars) {
		Trc_PRT_sysinfo_get_cwd_failed_getcurrentdirectory(GetLastError());
		portLibrary->mem_free_memory(portLibrary, unicodeBuffer);
		return -1;
	}

	/* convert to UTF-8 */
	portConvertRC = port_convertToUTF8(portLibrary, unicodeBuffer, buf, bufLen);

	portLibrary->mem_free_memory(portLibrary, unicodeBuffer);

	if (0 != portConvertRC) {
		Trc_PRT_sysinfo_get_cwd_failed_unicode_to_utf8();
		return -1;
	}

	return 0;
}

intptr_t
omrsysinfo_get_tmp(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, BOOLEAN ignoreEnvVariable)
{
	DWORD pathLengthInWChars = 0;
	wchar_t *unicodeBuffer = NULL;
	int32_t mutf8Size = -1;
	DWORD unicodeBufferSizeBytes = 0;

	if (NULL == buf) {
		Assert_PRT_true(0 == bufLen);
	}

	/* GetTempPathW returns the numbers of TCHARS (not including the terminating null character) needed to store string. The returned string ends with a backslash
	 *     - seeing as we always operate on systems that support, TCHAR is in fact wchar_t
	 *     - see "Working with Strings"  on MSDN for more information
	 *     Note that the function does not verify that the path exists
	 */
	pathLengthInWChars = GetTempPathW(0, NULL);

	if (0 == pathLengthInWChars) {
		/* do a trace point to provide the error code of GetCurrentDirectory */
		Trc_PRT_sysinfo_get_tmp_failed_gettemppathw(GetLastError());
		return -1;
	}

	unicodeBufferSizeBytes = (pathLengthInWChars + 1) * sizeof(wchar_t);

	/* we need a buffer to store the unicode string, as we'll use the supplied buf for the UTF8 conversion */
	unicodeBuffer = portLibrary->mem_allocate_memory(portLibrary, unicodeBufferSizeBytes, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);

	if (NULL == unicodeBuffer) {
		Trc_PRT_sysinfo_get_tmp_oome();
		return -1;
	}

	pathLengthInWChars = GetTempPathW(pathLengthInWChars + 1, unicodeBuffer);

	if (0 == pathLengthInWChars) {
		Trc_PRT_sysinfo_get_tmp_failed_gettemppathw(GetLastError());
		portLibrary->mem_free_memory(portLibrary, unicodeBuffer);
		return -1;
	}

	/* convert to modified UTF-8. */
	mutf8Size = portLibrary->str_convert(portLibrary, J9STR_CODE_WIDE, J9STR_CODE_MUTF8, (const char *) unicodeBuffer, (uintptr_t) pathLengthInWChars * sizeof(wchar_t), NULL, 0);
	if (mutf8Size >= 0) {
		mutf8Size += 1; /* leave enough space to null-terminate the string */
		/*is buflen big enough? */
		if (bufLen < (uintptr_t)mutf8Size) {
			portLibrary->mem_free_memory(portLibrary, unicodeBuffer);
			return (intptr_t)mutf8Size;
		} else {
			mutf8Size = portLibrary->str_convert(portLibrary, J9STR_CODE_WIDE, J9STR_CODE_MUTF8, (const char *) unicodeBuffer, (uintptr_t) pathLengthInWChars * sizeof(wchar_t), buf, bufLen);
			if (mutf8Size < 0) {
				Trc_PRT_sysinfo_get_tmp_failed_str_covert(mutf8Size);
			}
		}
	} else {
		Trc_PRT_sysinfo_get_tmp_failed_str_covert(mutf8Size);
	}

	portLibrary->mem_free_memory(portLibrary, unicodeBuffer);
	return 0;
}

int32_t
omrsysinfo_get_open_file_count(struct OMRPortLibrary *portLibrary, uint64_t *count)
{
	return OMRPORT_ERROR_SYSINFO_GET_OPEN_FILES_NOT_SUPPORTED;
}

intptr_t
omrsysinfo_get_os_description(struct OMRPortLibrary *portLibrary, struct OMROSDesc *desc)
{
	intptr_t rc = -1;
	Trc_PRT_sysinfo_get_os_description_Entered(desc);

	if (NULL != desc) {
		memset(desc, 0, sizeof(OMROSDesc));
	}

	Trc_PRT_sysinfo_get_os_description_Exit(rc);
	return rc;
}

BOOLEAN
omrsysinfo_os_has_feature(struct OMRPortLibrary *portLibrary, struct OMROSDesc *desc, uint32_t feature)
{
	BOOLEAN rc = FALSE;
	Trc_PRT_sysinfo_os_has_feature_Entered(desc, feature);

	if ((NULL != desc) && (feature < (OMRPORT_SYSINFO_OS_FEATURES_SIZE * 32))) {
		uint32_t featureIndex = feature / 32;
		uint32_t featureShift = feature % 32;

		rc = OMR_ARE_ALL_BITS_SET(desc->features[featureIndex], 1 << featureShift);
	}

	Trc_PRT_sysinfo_os_has_feature_Exit((uintptr_t)rc);
	return rc;
}

BOOLEAN
omrsysinfo_os_kernel_info(struct OMRPortLibrary *portLibrary, struct OMROSKernelInfo *kernelInfo)
{
	return FALSE;
}

BOOLEAN
omrsysinfo_cgroup_is_system_available(struct OMRPortLibrary *portLibrary)
{
	return FALSE;
}

uint64_t
omrsysinfo_cgroup_get_available_subsystems(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

uint64_t
omrsysinfo_cgroup_are_subsystems_available(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlags)
{
	return 0;
}

uint64_t
omrsysinfo_cgroup_get_enabled_subsystems(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

uint64_t
omrsysinfo_cgroup_enable_subsystems(struct OMRPortLibrary *portLibrary, uint64_t requestedSubsystems)
{
	return 0;
}

uint64_t
omrsysinfo_cgroup_are_subsystems_enabled(struct OMRPortLibrary *portLibrary, uint64_t subsystemsFlags)
{
	return 0;
}

int32_t
omrsysinfo_cgroup_get_memlimit(struct OMRPortLibrary *portLibrary, uint64_t *limit)
{
	return OMRPORT_ERROR_SYSINFO_CGROUP_UNSUPPORTED_PLATFORM;
}
