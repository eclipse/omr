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
 * @brief Dump formatting
 */

/* Turn off FPO optimisation on Windows 32-bit to improve native stack traces (CMVC 199392) */
#pragma optimize("y",off)

#include <windows.h>
#include <winnt.h>
#include <stdlib.h>
#include <float.h>
#include <process.h>
#include <dbghelp.h>

#include "omrport.h"
#include "omrsignal.h"
#include "omrutil.h"
#include "omrportpriv.h"
#include "omrportpg.h"

typedef BOOL (WINAPI *PMINIDUMPWRITEDUMP)(
	IN HANDLE hProcess,
	IN DWORD ProcessId,
	IN HANDLE hFile,
	IN MINIDUMP_TYPE DumpType,
	IN CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, OPTIONAL
	IN CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, OPTIONAL
	IN CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam OPTIONAL);

PMINIDUMPWRITEDUMP dump_fn;

#define MINIDUMPWRITEDUMP "MiniDumpWriteDump"
#define DBGHELP_DLL "DBGHELP.DLL"

typedef struct _WriteDumpFileArgs {
	PMINIDUMPWRITEDUMP dmp_function;
	HANDLE hDumpFile;
	MINIDUMP_EXCEPTION_INFORMATION *mdei;
	DWORD errorCode;
} WriteDumpFileArgs, *PWriteDumpFileArgs;

#define J9USE_UNIQUE_DUMP_NAMES "J9UNIQUE_DUMPS"
#define DUMP_FNAME_KEY "SOFTWARE\\Microsoft\\DrWatson"
#define DUMP_FNAME_VALUE "CrashDumpFile"

static HANDLE openFileInCWD(struct OMRPortLibrary *portLibrary,	char *fileNameBuff, uint32_t fileNameBuffSize);
static HANDLE openFileFromEnvVar(struct OMRPortLibrary *portLibrary, char *envVarName, char *fileNameBuff, uint32_t fileNameBuffSize);
static HANDLE openFileFromReg(const char *keyName, const char *valName, char *fileNameBuff, uint32_t fileNameBuffSize);
static void writeDumpFile(PWriteDumpFileArgs args);
static HINSTANCE loadDumpLib(void);
static PMINIDUMPWRITEDUMP linkDumpFn(HINSTANCE dllHandle, const char *fnName);
static int32_t convertError(DWORD errorCode);

/**
 * Create a dump file of the OS state.
 *
 * @param[in] portLibrary The port library.
 * @param[in] filename Buffer for filename optionally containing the filename where dump is to be output.
 * @param[out] filename filename used for dump file or error message.
 * @param[in] dumpType Type of dump to perform.
 * @param[in] userData Implementation specific data.
 *
 * @return 0 on success, non-zero otherwise.
 *
 * @note filename buffer can not be NULL.
 * @note user allocates and frees filename buffer.
 * @note filename buffer length is platform dependent, assumed to be EsMaxPath/MAX_PATH
 *
 * @note if filename buffer is empty, a filename will be generated.
 * @note if J9UNIQUE_DUMPS is set, filename will be unique.
 */
uintptr_t
omrdump_create(struct OMRPortLibrary *portLibrary, char *filename, char *dumpType, void *userData)
{
	HANDLE hFile;
	WriteDumpFileArgs args;
	HINSTANCE dllHandle;
	HANDLE hThread;
	MINIDUMP_EXCEPTION_INFORMATION mdei;
	EXCEPTION_POINTERS exceptionPointers;
	J9Win32SignalInfo *info = (J9Win32SignalInfo *)userData;

	if (filename == NULL) {
		return 1;
	}

	if (*filename == '\0') {
		/* no file name provided, so generate one and open the file */

		hFile = openFileFromReg(DUMP_FNAME_KEY, DUMP_FNAME_VALUE, filename, EsMaxPath);
		if (hFile == INVALID_HANDLE_VALUE) {
			hFile = openFileFromEnvVar(portLibrary, "USERPROFILE", filename, EsMaxPath);
			if (hFile == INVALID_HANDLE_VALUE) {
				hFile = openFileInCWD(portLibrary, filename, EsMaxPath);
				if (hFile == INVALID_HANDLE_VALUE) {
					hFile = openFileFromEnvVar(portLibrary, "TEMP", filename, EsMaxPath);
					if (hFile == INVALID_HANDLE_VALUE) {
						hFile = openFileFromEnvVar(portLibrary, "TMP", filename, EsMaxPath);
					}
				}
			}
		}

	} else {
		/* use name provided */
		hFile = (HANDLE) portLibrary->file_open(portLibrary, filename, EsOpenWrite | EsOpenCreate | EsOpenRead, 0666);   /* 0666 is a guess...0?, 0666??...want to grant write permissions */
	}

	if (hFile == (HANDLE)-1) {
		portLibrary->str_printf(portLibrary, filename, EsMaxPath, "Dump failed - could not create dump file:\n"); /*make sure useful*/
		return 1;
	}

	dllHandle = loadDumpLib();

	if (dllHandle == NULL) {
		portLibrary->str_printf(portLibrary, filename, EsMaxPath, "Dump failed - could not load library %s\n", DBGHELP_DLL);
		return 1;
	}

	dump_fn = linkDumpFn(dllHandle, MINIDUMPWRITEDUMP);

	if (dump_fn == NULL) {
		portLibrary->str_printf(portLibrary, filename, EsMaxPath, "Dump failed - could not link %s in %s\n", MINIDUMPWRITEDUMP, DBGHELP_DLL);
		return 1;
	}

	/* collect exception data */
	if (info) {
		exceptionPointers.ExceptionRecord = info->ExceptionRecord;
		exceptionPointers.ContextRecord = info->ContextRecord;
		mdei.ThreadId = GetCurrentThreadId();
		mdei.ExceptionPointers = &exceptionPointers;
		mdei.ClientPointers = TRUE;
	}

	args.dmp_function = dump_fn;
	args.hDumpFile = hFile;
	args.mdei = info ? &mdei : NULL;
	args.errorCode = 0;

	/* call createCrashDumpFile on a different thread so we can walk the stack back to our code */
	hThread = (HANDLE)_beginthread(writeDumpFile, 0, &args);

	/* exception thread waits while crash dump is printed on other thread */
	if ((HANDLE)-1 == hThread) {
		portLibrary->str_printf(portLibrary, filename, EsMaxPath, "Dump failed - could not begin dump thread\n");
		return 1;
	} else {
		WaitForSingleObject(hThread, INFINITE);
	}

	if ((portLibrary->file_close(portLibrary, (int32_t)(intptr_t)hFile)) == -1) {
		portLibrary->str_printf(portLibrary, filename, EsMaxPath, "Dump failed - could not close dump file.\n"); /*make sure useful*/
		return 1;
	}

	/* Check for errors passed back from the dump thread via the WriteDumpFileArgs args structure */
	if (0 != args.errorCode) {
		portLibrary->error_set_last_error(portLibrary, args.errorCode, convertError(args.errorCode));
		portLibrary->str_printf(portLibrary, filename, EsMaxPath, "Dump failed, error code %s", portLibrary->error_last_error_message(portLibrary));
		return 1;
	}

	return 0;
}


static PMINIDUMPWRITEDUMP
linkDumpFn(HINSTANCE dllHandle, const char *fnName)
{
	return (PMINIDUMPWRITEDUMP)GetProcAddress(dllHandle, fnName);
}


/* will remove private from name once duplicate functionality removed from GPHandler */
static HINSTANCE
loadDumpLib(void)
{
	HINSTANCE dbghelpDll = (HINSTANCE)j9getdbghelp_getDLL();

	if (dbghelpDll == NULL) {
		dbghelpDll = (HINSTANCE)j9getdbghelp_loadDLL();
	}

	return dbghelpDll;
}

/**
 * Calls the Windows MiniDumpWriteDump API to create a core dump. This function is run on its own thread.
 *
 * @param[in] args  pointer to a WriteDumpFileArgs structure containing the MiniDumpWriteDump function pointer,
 * 					dump file handle, dump parameters and return code
 * @return  none
 */
static void
writeDumpFile(PWriteDumpFileArgs args)
{
#define MAX_MINIDUMP_ATTEMPTS  10

	HANDLE hFile = args->hDumpFile;
	int i;

	for (i = 0; i < MAX_MINIDUMP_ATTEMPTS; i++) {
		BOOL retVal;

		args->errorCode = 0; /* reset the error code on each retry */
		/* Dump flags match the defaults used when a dump is created via Windows Task Manager.
		 * The dump flags for a dump can be viewed in windbg using the ".dumpdebug" command.
		 * (The command is not documented in the help but will display the dump flags and streams
		 * included in the dump.)
		 */
		retVal = args->dmp_function(
					 GetCurrentProcess(),
					 GetCurrentProcessId(),
					 hFile,
					 MiniDumpWithFullMemory | MiniDumpWithHandleData |
					 MiniDumpWithUnloadedModules | MiniDumpWithFullMemoryInfo |
					 MiniDumpWithThreadInfo,
					 args->mdei,
					 NULL,
					 NULL);

		if (TRUE == retVal) {
			break;	/* dump taken! */
		} else {
			args->errorCode = GetLastError(); /* retrieve Windows error code (HRESULT) */
			if (args->errorCode == HRESULT_FROM_WIN32(ERROR_DISK_FULL)) {
				break; /* don't attempt retry if disk is full, bail out now */
			}
			Sleep(100);
		}
	}

	_endthread();

}


static HANDLE
openFileFromEnvVar(struct OMRPortLibrary *portLibrary, char *envVarName, char *fileNameBuff, uint32_t fileNameBuffSize)
{
	HANDLE hFile;
	intptr_t retCode;
	uint32_t i;
	char pidStore[25];	/* This is roughly the 3 * sizeof(int)+1 which is more than enough space*/

	if (portLibrary == NULL) {
		return INVALID_HANDLE_VALUE;
	}

	retCode = portLibrary->sysinfo_get_env(portLibrary, envVarName, fileNameBuff, fileNameBuffSize);
	if (retCode != 0) {
		return INVALID_HANDLE_VALUE;
	}

	retCode = portLibrary->sysinfo_get_env(portLibrary, J9USE_UNIQUE_DUMP_NAMES, NULL, 0);
	if (-1 != retCode) {
		pidStore[0] = '-';
		/* this env var is set to append a unique identifier to the dump file name */
		_itoa(GetCurrentProcessId(), &pidStore[1], 10);
	} else {
		/* we aren't using this feature so make this a zero-length string */
		pidStore[0] = 0;
	}

	i = (uint32_t)strlen(fileNameBuff);
	i += (uint32_t)strlen(pidStore);
	if (i + 8 > fileNameBuffSize) {
		return INVALID_HANDLE_VALUE;
	}
	strcat(fileNameBuff, "\\j9");
	strcat(fileNameBuff, pidStore);
	strcat(fileNameBuff, ".dmp");

	hFile = CreateFileA(
				fileNameBuff,
				GENERIC_READ | GENERIC_WRITE,
				0,
				NULL,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL);

	return hFile;
}


static HANDLE
openFileFromReg(const char *keyName, const char *valName, char *fileNameBuff, uint32_t fileNameBuffSize)
{
	HANDLE hFile;
	HKEY hKey;
	LONG lRet;
	uint32_t buffUsed = fileNameBuffSize;

	lRet = RegOpenKeyEx(
			   HKEY_LOCAL_MACHINE,
			   keyName,
			   0,
			   KEY_QUERY_VALUE,
			   &hKey);
	if (lRet != ERROR_SUCCESS) {
		return INVALID_HANDLE_VALUE;
	}

	lRet = RegQueryValueEx(
			   hKey,
			   valName,
			   NULL,
			   NULL,
			   (LPBYTE)fileNameBuff,
			   &buffUsed);
	if (lRet != ERROR_SUCCESS) {
		return INVALID_HANDLE_VALUE;
	}

	RegCloseKey(hKey);

	hFile = CreateFileA(
				fileNameBuff,
				GENERIC_READ | GENERIC_WRITE,
				0,
				NULL,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL);

	return hFile;
}


static HANDLE
openFileInCWD(struct OMRPortLibrary *portLibrary, char *fileNameBuff, uint32_t fileNameBuffSize)
{
	HANDLE hFile;
	uint32_t length;

	if (portLibrary == NULL) {
		return INVALID_HANDLE_VALUE;
	}

	length = GetModuleFileName(NULL, fileNameBuff, fileNameBuffSize);
	if (length == 0 || length > fileNameBuffSize || length < 3) {
		return INVALID_HANDLE_VALUE;
	}
	fileNameBuff[length - 1] = 'p';
	fileNameBuff[length - 2] = 'm';
	fileNameBuff[length - 3] = 'd';

	hFile = CreateFileA(
				fileNameBuff,
				GENERIC_READ | GENERIC_WRITE,
				0,
				NULL,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL);

	return hFile;
}

int32_t
omrdump_startup(struct OMRPortLibrary *portLibrary)
{
	/* Allocate debug library function table */
	PPG_dbgHlpLibraryFunctions = (Dbg_Entrypoints *)portLibrary->mem_allocate_memory(portLibrary, sizeof(Dbg_Entrypoints), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == PPG_dbgHlpLibraryFunctions) {
		return OMRPORT_ERROR_STARTUP_DUMP;
	}

	memset(PPG_dbgHlpLibraryFunctions, '\0', sizeof(Dbg_Entrypoints));

	return 0;
}

void
omrdump_shutdown(struct OMRPortLibrary *portLibrary)
{
	/* Free debug library function table */
	if (NULL != PPG_dbgHlpLibraryFunctions) {
		portLibrary->mem_free_memory(portLibrary, PPG_dbgHlpLibraryFunctions);
	}
}

/**
 * Converts a Windows HRESULT error code into a J9 Port Library error code
 *
 * @param[in] errorCode The HRESULT error code reported by Windows
 *
 * @return the J9 Port Library error code
 */
static int32_t
convertError(DWORD errorCode)
{
	if (errorCode == HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE)) {
		return OMRPORT_ERROR_FILE_NAMETOOLONG;
	} else if (errorCode == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)) {
		return OMRPORT_ERROR_FILE_NOPERMISSION;
	} else if (errorCode == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
		return OMRPORT_ERROR_FILE_NOENT;
	} else if (errorCode == HRESULT_FROM_WIN32(ERROR_DISK_FULL)) {
		return OMRPORT_ERROR_FILE_DISKFULL;
	} else if (errorCode == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS)) {
		return OMRPORT_ERROR_FILE_EXIST;
	} else if (errorCode == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
		return OMRPORT_ERROR_FILE_EXIST;
	} else if (errorCode == HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY)) {
		return OMRPORT_ERROR_FILE_SYSTEMFULL;
	} else if (errorCode == HRESULT_FROM_WIN32(ERROR_LOCK_VIOLATION)) {
		return OMRPORT_ERROR_FILE_LOCK_NOREADWRITE;
	} else if (errorCode == HRESULT_FROM_WIN32(ERROR_NOT_READY)) {
		return OMRPORT_ERROR_FILE_IO;
	} else {
		return OMRPORT_ERROR_FILE_OPFAILED;
	}
}

