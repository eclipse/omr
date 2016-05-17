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
 * @brief shared library
 */

#include <windows.h>
#include "omrportptb.h"
#include "omrport.h"
#include "omrportpriv.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "omr.h"
#include "omrutil.h"
#include "portnls.h"
#include "ut_omrport.h"

#ifndef HINSTANCE_ERROR
#define HINSTANCE_ERROR 32
#endif

/* Error codes used by checkLibraryType() for incompatible library architecture */
enum {
	LIB_UNSUPPORTED_MAGIC = 1,
	LIB_UNSUPPORTED_SIGNATURE,
	LIB_UNSUPPORTED_MACHINE
};

static uintptr_t EsSharedLibraryLookupName(uintptr_t descriptor, char *name, uintptr_t *func);
static int32_t findError(int32_t errorCode);

static uintptr_t
EsSharedLibraryLookupName(uintptr_t descriptor, char *name, uintptr_t *func)
{
	uintptr_t lpfnFunction;

	if (descriptor < HINSTANCE_ERROR) {
		return 3;
	}
	lpfnFunction = (uintptr_t)GetProcAddress((HINSTANCE)descriptor, (LPCSTR)name);
	if ((uintptr_t)NULL == lpfnFunction) {
		return 4;
	}
	*func = lpfnFunction;
	return 0;
}

uintptr_t
omrsl_open_shared_library(struct OMRPortLibrary *portLibrary, char *name, uintptr_t *descriptor, uintptr_t flags)
{
	HINSTANCE dllHandle;
	UINT prevMode;
	DWORD error;
	uintptr_t notFound;
	const char *errorMessage = NULL;
	char errBuf[512];
	char mangledName[EsMaxPath + 1];
	char *openName = name;
	wchar_t portLibDir[EsMaxPath];
	wchar_t unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodeName;
	BOOLEAN decorate = J9_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_DECORATE);
	BOOLEAN openExec = J9_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_OPEN_EXECUTABLE);
	uintptr_t pathLength = 0;

	Trc_PRT_sl_open_shared_library_Entry(name, flags);

	/* Suppress error message alert. */
	prevMode = SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

	/* No need to name mangle if a handle to the executable is requested for. */
	if (!openExec) {
		if (decorate) {
			pathLength = portLibrary->str_printf(portLibrary, mangledName, (EsMaxPath + 1), "%s.dll", name);
			if (pathLength >= EsMaxPath) {
				Trc_PRT_sl_open_shared_library_Exit2(OMRPORT_SL_UNSUPPORTED);
				return OMRPORT_SL_UNSUPPORTED;
			}
			openName = mangledName;
		} /* TODO make windows not try to append .dll if we do not want it to */
	} else {
		/* Open a handle to the executable that launched the virtual machine. */
		dllHandle = GetModuleHandle(NULL);

		/* Check for any errors. */
		if (NULL == dllHandle) {
			BOOLEAN useNlsMessage = J9_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_NO_LOOKUP_MSG_FOR_NOT_FOUND);
			BOOLEAN platformErrorFailed = FALSE;

			error = GetLastError();
			/* Check whether a system error message is requested or not, since retrieving system based
			 * error messages can be costly.  If not, use the NLS message instead.  Also, if system
			 * message retrieval failed, use NLS messages anyway.
			 */
			if (!useNlsMessage) {
				wchar_t *errorString = NULL;
				DWORD msgFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
								 | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_MAX_WIDTH_MASK;
				int rc = 0;

				rc = FormatMessageW(msgFlags,
									NULL,
									error, /* The error code from above */
									MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
									(LPWSTR) &errorString,
									0,
									NULL);

				if ((0 != rc) && (NULL != errorString)) {
					port_convertToUTF8(portLibrary, errorString, errBuf, sizeof(errBuf) - 1);
					LocalFree(errorString);
					errBuf[sizeof(errBuf) - 1] = '\0';
				} else {
					platformErrorFailed = TRUE;
				}
			}

			if (useNlsMessage || platformErrorFailed) {
				errorMessage = portLibrary->nls_lookup_message(portLibrary,
							   J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
							   J9NLS_PORT_SL_EXECUTABLE_OPEN_ERROR,
							   NULL);
				portLibrary->str_printf(portLibrary, errBuf, sizeof(errBuf), errorMessage, error);
				errBuf[sizeof(errBuf) - 1] = '\0';
			}

			Trc_PRT_sl_open_shared_library_Event2(errBuf);
			Trc_PRT_sl_open_shared_library_Exit2(OMRPORT_SL_NOT_FOUND);
			return portLibrary->error_set_last_error(portLibrary, error, findError(error));
		}

		Trc_PRT_sl_open_shared_library_Event1(NULL);
		*descriptor = (uintptr_t)dllHandle;
		Trc_PRT_sl_open_shared_library_Exit1(dllHandle);
		return OMRPORT_SL_FOUND;
	}

	/* Convert the filename from UTF8 to Unicode */
	unicodeName = port_convertFromUTF8(portLibrary, openName, unicodeBuffer, UNICODE_BUFFER_SIZE);
	if (NULL == unicodeName) {
		Trc_PRT_sl_open_shared_library_Exit2(-1);
		return -1;
	}

	Trc_PRT_sl_open_shared_library_Event1(openName);

	/* LoadLibrary will try appending .DLL if necessary */
	dllHandle = LoadLibraryW(unicodeName);
	if (dllHandle >= (HINSTANCE)HINSTANCE_ERROR) {
		*descriptor = (uintptr_t)dllHandle;
		SetErrorMode(prevMode);
		if (unicodeBuffer != unicodeName) {
			portLibrary->mem_free_memory(portLibrary, unicodeName);
		}
		Trc_PRT_sl_open_shared_library_Exit1(dllHandle);
		return 0;
	}

	/* Now try with "altered search path" to catch DLLs with dependencies */
	dllHandle = LoadLibraryExW(unicodeName, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (dllHandle >= (HINSTANCE)HINSTANCE_ERROR) {
		*descriptor = (uintptr_t)dllHandle;
		SetErrorMode(prevMode);
		if (unicodeBuffer != unicodeName) {
			portLibrary->mem_free_memory(portLibrary, unicodeName);
		}
		Trc_PRT_sl_open_shared_library_Exit1(dllHandle);
		return 0;
	}

	/* record the error */
	error = GetLastError();

	/* try again in the port lib dir */
	{
		wchar_t *cursor = NULL;

		if (OMR_ERROR_NONE == detectVMDirectory(portLibDir, EsMaxPath - 2, &cursor)) {
			if (NULL != cursor) {
				/* detectVMDirectory ensures (wcslen(portLibDir) < (EsMaxPath - 2)) so length > 0 */
				size_t length = (EsMaxPath - 2) - wcslen(portLibDir);
				_snwprintf(cursor, length, L"\\%s", unicodeName);
				portLibDir[EsMaxPath - 1] = L'\0';
				dllHandle = LoadLibraryExW(portLibDir, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
				if (dllHandle >= (HINSTANCE)HINSTANCE_ERROR) {
					*descriptor = (uintptr_t)dllHandle;
					SetErrorMode(prevMode);
					if (unicodeBuffer != unicodeName) {
						portLibrary->mem_free_memory(portLibrary, unicodeName);
					}
					Trc_PRT_sl_open_shared_library_Exit1(dllHandle);
					return 0;
				}
			}
		}
	}

	/*
	 * CVMC 201408:
	 * When VM is launched from a network share drive, ERROR_BAD_PATHNAME is generated by loadLibrary() on Windows
	 * if an invalid quoted path is detected in the library path list (-Djava.library.path=). In such case, VM fails
	 * to process the remaining paths and exits immediately. To fix up the issue, ERROR_BAD_PATHNAME should be covered
	 * in the error code list below so as to proceed with the searching.
	 */
	notFound = ((ERROR_MOD_NOT_FOUND == error) || (ERROR_DLL_NOT_FOUND == error) || (ERROR_BAD_PATHNAME == error));
	if (notFound) {
		/* try to report a better error message.  Check if the library can be found at all. */
		dllHandle = LoadLibraryExW(unicodeName, NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_WITH_ALTERED_SEARCH_PATH);
		if ((HINSTANCE)0 != dllHandle) {
			if (0 != sizeof(errBuf)) {
				errorMessage = portLibrary->nls_lookup_message(portLibrary,
							   J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
							   J9NLS_PORT_SL_UNABLE_TO_RESOLVE_REFERENCES,
							   NULL);
				strncpy(errBuf, errorMessage, sizeof(errBuf));
				errBuf[sizeof(errBuf) - 1] = '\0';
				Trc_PRT_sl_open_shared_library_Event2(errBuf);
			}
			FreeLibrary(dllHandle);
			SetErrorMode(prevMode);
			if (unicodeBuffer != unicodeName) {
				portLibrary->mem_free_memory(portLibrary, unicodeName);
			}
			Trc_PRT_sl_open_shared_library_Exit2(OMRPORT_SL_INVALID);
			return portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_INVALID, errBuf);
		}
		/* When the flag OMRPORT_SLOPEN_NO_LOOKUP_MSG_FOR_NOT_FOUND is set and the error is ERROR_MOD_NOT_FOUND,
		 * do not call FormatMessageW() since it cost 700K of memory when called the first time. */
		if ((ERROR_MOD_NOT_FOUND == error) && J9_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_NO_LOOKUP_MSG_FOR_NOT_FOUND)) {
			const char errorString[] = "The specified module could not be found";
			SetErrorMode(prevMode);
			if (unicodeBuffer != unicodeName) {
				portLibrary->mem_free_memory(portLibrary, unicodeName);
			}
			memcpy(errBuf, errorString, sizeof(errorString));
			Trc_PRT_sl_open_shared_library_Event2(errBuf);
			Trc_PRT_sl_open_shared_library_Exit2(OMRPORT_SL_NOT_FOUND);
			return portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_NOT_FOUND, errBuf);
		}
	}

	if (unicodeBuffer != unicodeName) {
		portLibrary->mem_free_memory(portLibrary, unicodeName);
	}

	if (0 != sizeof(errBuf)) {
		wchar_t *message = NULL;
		int nameSize = (int)strlen(name);

		unicodeName = port_convertFromUTF8(portLibrary, openName, unicodeBuffer, UNICODE_BUFFER_SIZE);
		if (NULL == unicodeName) {
			errorMessage = portLibrary->nls_lookup_message(portLibrary,
						   J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
						   J9NLS_PORT_SL_INTERNAL_ERROR,
						   NULL);
			portLibrary->str_printf(portLibrary, errBuf, sizeof(errBuf), errorMessage, error);
			errBuf[sizeof(errBuf) - 1] = '\0';
			SetErrorMode(prevMode);
			Trc_PRT_sl_open_shared_library_Exit3(nameSize * 2 + 2, notFound ? OMRPORT_SL_NOT_FOUND : OMRPORT_SL_INVALID);
			return portLibrary->error_set_last_error_with_message(portLibrary, notFound ? OMRPORT_SL_NOT_FOUND : OMRPORT_SL_INVALID, errBuf);
		}

		FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_MAX_WIDTH_MASK,
			NULL,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
			(LPWSTR) &message,
			0,
			(va_list *)&unicodeName);

		if (unicodeBuffer != unicodeName) {
			portLibrary->mem_free_memory(portLibrary, unicodeName);
		}

		if (NULL != message) {
			port_convertToUTF8(portLibrary, message, errBuf, sizeof(errBuf) - 1);
			LocalFree(message);
		} else {
			errorMessage = portLibrary->nls_lookup_message(portLibrary,
						   J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
						   J9NLS_PORT_SL_INTERNAL_ERROR,
						   NULL);
			portLibrary->str_printf(portLibrary, errBuf, sizeof(errBuf), errorMessage, error);
		}
		errBuf[sizeof(errBuf) - 1] = '\0';
		Trc_PRT_sl_open_shared_library_Event2(errBuf);
	}

	SetErrorMode(prevMode);
	Trc_PRT_sl_open_shared_library_Exit2(notFound ? OMRPORT_SL_NOT_FOUND : OMRPORT_SL_INVALID);
	return portLibrary->error_set_last_error_with_message(portLibrary, notFound ? OMRPORT_SL_NOT_FOUND : OMRPORT_SL_INVALID, errBuf);
}

/**
 * Close a shared library.
 *
 * @param[in] portLibrary The port library.
 * @param[in] descriptor Shared library handle to close.
 *
 * @return 0 on success, any other value on failure.
 */
uintptr_t
omrsl_close_shared_library(struct OMRPortLibrary *portLibrary, uintptr_t descriptor)
{
	Trc_PRT_sl_close_shared_library_Entry(descriptor);

	if (descriptor < HINSTANCE_ERROR) {
		Trc_PRT_sl_close_shared_library_Exit(2);
		return 2;
	}
	FreeLibrary((HINSTANCE)descriptor);

	Trc_PRT_sl_close_shared_library_Exit(0);
	return 0;
}

/**
 * Search for a function named 'name' taking argCount in the shared library 'descriptor'.
 *
 * @param[in] portLibrary The port library.
 * @param[in] descriptor Shared library to search.
 * @param[in] name Function to look up.
 * @param[out] func Pointer to the function.
 * @param[in] argSignature Argument signature.
 *
 * @return 0 on success, any other value on failure.
 *
 * argSignature is a C (ie: NUL-terminated) string with the following possible values for each character:
 *
 *      V - void
 *      Z - boolean
 *      B - byte
 *      C - char (16 bits)
 *      I - integer (32 bits)
 *      J - long (64 bits)
 *      F - float (32 bits)
 *      D - double (64 bits)
 *      L - object / pointer (32 or 64, depending on platform)
 *      P - pointer-width platform data. (in J9 terms an intptr_t)
 *
 * Lower case signature characters imply unsigned value.
 * Upper case signature characters imply signed values.
 * If it doesn't make sense to be signed/unsigned (eg: V, L, F, D Z) the character is upper case.
 *
 * argList[0] is the return type from the function.
 * The argument list is as it appears in english: list is left (1) to right (argCount)
 *
 * @note contents of func are undefined on failure.
 */
uintptr_t
omrsl_lookup_name(struct OMRPortLibrary *portLibrary, uintptr_t descriptor, char *name, uintptr_t *func, const char *argSignature)
{
	uintptr_t result;

	Trc_PRT_sl_lookup_name_Entry(descriptor, name, argSignature);

	result = EsSharedLibraryLookupName(descriptor, name, func);
	if (0 != result) {
		size_t i = 0;
		size_t len = 0;
		size_t pad = 0;

		/* + 7 comes from: 256 parameters * 8 bytes each = 2048 bytes -> 4 digits + @ symbol + leading '_' + NULL */
		size_t length = sizeof(char) * (strlen(name) + 7);
		char *mangledName = (char *)alloca(length);
		if (NULL == mangledName) {
			Trc_PRT_sl_lookup_name_Exit3(length, 1);
			return 1;
		}

		len = strlen(argSignature);

		for (i = 1; i < len; i++) {
			if (('J' == argSignature[i]) || ('D' == argSignature[i])) {
				pad++;
			}
		}

		portLibrary->str_printf(portLibrary, mangledName, length, "_%s@%d", name, (len + pad - 1) * 4);
		result = EsSharedLibraryLookupName(descriptor, mangledName, func);
	}

	if (0 == result) {
		Trc_PRT_sl_lookup_name_Exit1(*func);
	} else {
		Trc_PRT_sl_lookup_name_Exit2(name, argSignature, descriptor, result);
	}
	return result;
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrsl_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library.
 *
 * @note Most implementations will be empty.
 */
void
omrsl_shutdown(struct OMRPortLibrary *portLibrary)
{
}

/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the shared library operations may be created here.  All resources created here should be destroyed
 * in @ref omrsl_shutdown.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_SL
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrsl_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

int32_t
findError(int32_t errorCode)
{
	/* As of now, we don't distinguish between most of the Windows DLL-related error
	 * codes, but translate most of these to an OMRPORT_SL_INVALID.
	 */
	switch (errorCode) {
	case ERROR_DLL_NOT_FOUND: /* Fall through */
	case ERROR_BAD_PATHNAME:  /* Fall through */
	case ERROR_MOD_NOT_FOUND:
		return OMRPORT_SL_INVALID;
	default:
		return OMRPORT_SL_UNKNOWN;
	}
}
