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


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dll.h>
#include <stdlib.h> /* for malloc */

#include "omrport.h"
#include "portnls.h"
#include "ut_omrport.h"

#define DMESSAGE(x)				/* printf x; */


static void getDLError(struct OMRPortLibrary *portLibrary, char *errBuf, uintptr_t bufLen);

uintptr_t
omrsl_open_shared_library(struct OMRPortLibrary *portLibrary, char *name, uintptr_t *descriptor, uintptr_t flags)
{
	void *handle;
	char *openName = name;
	char mangledName[EsMaxPath + 1];
	char errBuf[512];
	BOOLEAN decorate = J9_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_DECORATE);
	BOOLEAN openExec = J9_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_OPEN_EXECUTABLE);
	uintptr_t pathLength = 0;

	Trc_PRT_sl_open_shared_library_Entry(name, flags);

	/* No need to name mangle if a handle to the executable is requested for. */
	if (!openExec && decorate) {
		char *p = strrchr(name, '/');
		if (p) {
			/* the names specifies a path */
			pathLength = portLibrary->str_printf(portLibrary, mangledName, (EsMaxPath + 1), "%.*slib%s.so", (uintptr_t)p + 1 - (uintptr_t)name, name, p + 1);
		} else {
			pathLength = portLibrary->str_printf(portLibrary, mangledName, (EsMaxPath + 1), "lib%s.so", name);
		}
		if (pathLength >= EsMaxPath) {
			Trc_PRT_sl_open_shared_library_Exit2(OMRPORT_SL_UNSUPPORTED);
			return OMRPORT_SL_UNSUPPORTED;
		}
		openName = mangledName;
	}

	Trc_PRT_sl_open_shared_library_Event1(openName);

	/* dllload() can open a handle to the executable with its name itself (unlike NULL
	 * on other platforms) provided the executable is itself built as a DLL (in XPLINK
	 * mode), which anyway is required for enabling symbol resolution.
	 */
	handle = dllload(openName);
	if (NULL == handle) {
		getDLError(portLibrary, errBuf, sizeof(errBuf));
		Trc_PRT_sl_open_shared_library_Event2(errBuf);
		if (portLibrary->file_attr(portLibrary, openName) == EsIsFile) {
			Trc_PRT_sl_open_shared_library_Exit2(OMRPORT_SL_INVALID);
			return portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_INVALID, errBuf);
		} else {
			Trc_PRT_sl_open_shared_library_Exit2(OMRPORT_SL_NOT_FOUND);
			return portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_SL_NOT_FOUND, errBuf);
		}
	}

	*descriptor = (uintptr_t)handle;
	Trc_PRT_sl_open_shared_library_Exit1(*descriptor);
	return 0;
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
	int error;
	dllhandle *handle;

	Trc_PRT_sl_close_shared_library_Entry(descriptor);

	DMESSAGE(("\nClose library %x\n", *descriptor))
	handle = (dllhandle *)descriptor;
	error = dllfree(handle);

	Trc_PRT_sl_close_shared_library_Exit(error);
	return error;
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
 *		V	- void
 *		Z	- boolean
 *		B	- byte
 *		C	- char (16 bits)
 *		I	- integer (32 bits)
 *		J	- long (64 bits)
 *		F	- float (32 bits)
 *		D	- double (64 bits)
 *		L	- object / pointer (32 or 64, depending on platform)
 *		P	- pointer-width platform data. (in J9 terms an intptr_t)
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
	void *address;
	dllhandle *handle;

	Trc_PRT_sl_lookup_name_Entry(descriptor, name, argSignature);

	handle = (dllhandle *)descriptor;
	address = (void *)dllqueryfn(handle, name);
	if (address == NULL) {
		Trc_PRT_sl_lookup_name_Exit2(name, argSignature, handle, 1);
		return 1;
	}
	*func = (uintptr_t)address;
	Trc_PRT_sl_lookup_name_Exit1(*func);
	return 0;
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
static void
getDLError(struct OMRPortLibrary *portLibrary, char *errBuf, uintptr_t bufLen)
{
	const char *error;

	if (bufLen == 0) {
		return;
	}

	error = strerror(errno);
	if (error == NULL || error[0] == '\0') {
		/* just in case another thread consumed our error message */
		error = portLibrary->nls_lookup_message(portLibrary,
												J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
												J9NLS_PORT_SL_UNKOWN_ERROR,
												NULL);
	}

	strncpy(errBuf, error, bufLen);
	errBuf[bufLen - 1] = '\0';
}


