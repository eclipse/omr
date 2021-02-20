/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

#if defined(J9ZOS39064)
#include "omrcel4ro31.h"
#endif /* defined(J9ZOS39064) */

#define DMESSAGE(x)				/* printf x; */


static void getDLError(struct OMRPortLibrary *portLibrary, char *errBuf, uintptr_t bufLen);

uintptr_t
omrsl_open_shared_library(struct OMRPortLibrary *portLibrary, char *name, uintptr_t *descriptor, uintptr_t flags)
{
	void *handle = NULL;
	char *openName = name;
	char mangledName[EsMaxPath + 1] = {0};
	char errBuf[512] = {0};
	BOOLEAN isErrBufPopulated = FALSE;
	BOOLEAN decorate = OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_DECORATE);
	BOOLEAN openExec = OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_OPEN_EXECUTABLE);

#if defined(J9ZOS39064)
	BOOLEAN attempt31BitDLLLoad = OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SLOPEN_ATTEMPT_31BIT_OPEN);
#endif /* defined(J9ZOS39064) */

	uintptr_t pathLength = 0;
	uintptr_t dirNameLength = 0;

	Trc_PRT_sl_open_shared_library_Entry(name, flags);

	/* No need to name mangle if a handle to the executable is requested for. */
	if (!openExec && decorate) {
		char *p = strrchr(name, '/');
		if (p) {
			/* the names specifies a path */
			dirNameLength = (uintptr_t)p + 1 - (uintptr_t)name;
			pathLength = portLibrary->str_printf(portLibrary, mangledName, (EsMaxPath + 1), "%.*slib%s.so", dirNameLength, name, p + 1);
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

	if ((NULL == handle) && !openExec && decorate && (0 != dirNameLength)) {
		/* z/OS doesn't support dladdr so we can't search the dir of the portLib for the dll
		 * as is the case on linux and macos. Instead, attempt to load the DLL with just its
		 * name and not the full path. dllload will search the filesystem for the library */
		openName +=  dirNameLength;

		Trc_PRT_sl_open_shared_library_Event2(openName);
		handle = dllload(openName);
	}

#if defined(J9ZOS39064)
	/* Attempt to load target library as 31-bit. */
	if (attempt31BitDLLLoad && (NULL == handle) && omr_cel4ro31_is_supported()) {
		OMR_CEL4RO31_infoBlock *ro31InfoBlock = NULL;

		/* Generate trace event for the error reason from preceding 64-bit dllload(). */
		getDLError(portLibrary, errBuf, sizeof(errBuf));
		Trc_PRT_sl_open_shared_library_Event2(errBuf);
		isErrBufPopulated = TRUE;

		DMESSAGE(("omrsl_open_shared_library: Attempt to load 31-bit DLL [%s]\n", openName))

		/* Due to temporary LE limitation, we need to pass environment variables via _CEE_ENVFILE.
		 * Since this could be the first path in establishing 31-bit secondary enclave,
		 * and launcher may unset _CEE_ENVFILE in order for child processes to
		 * pick up LIBPATH updates, we need to copy the _CEE3164_ENVFILE env var
		 * to _CEE_ENVFILE for duration of this CEL4RO31 invocation.
		 */
		char *CEE_ENVFILE_envvar = getenv("_CEE_ENVFILE");
		char *CEE3164_ENVFILE_envvar = getenv("_CEE3164_ENVFILE");
		BOOLEAN initCEE_ENVFILE = ((NULL == CEE_ENVFILE_envvar) || ('\0' == CEE_ENVFILE_envvar[0])) && (NULL != CEE3164_ENVFILE_envvar);

		if (initCEE_ENVFILE) {
			char buf[1024];
			snprintf(buf, sizeof(buf), "_CEE_ENVFILE=%s", CEE3164_ENVFILE_envvar);
			putenv(buf);
		}

		ro31InfoBlock = omr_cel4ro31_init(OMR_CEL4RO31_FLAG_LOAD_DLL, openName, NULL, 0);
		if (NULL != ro31InfoBlock) {
			OMR_CEL4RO31_controlBlock *ro31ControlBlock = &(ro31InfoBlock->ro31ControlBlock);

			omr_cel4ro31_call(ro31InfoBlock);

			if (OMR_CEL4RO31_RETCODE_OK == ro31ControlBlock->retcode) {
				/* Extract DLL handle from CEL4RO31 load result. */
				handle = (void *)ro31ControlBlock->dllHandle;
				/* High tag the handle to distingush 31-bit DLL. */
				handle = (void *)(OMRPORT_SL_ZOS_31BIT_TARGET_HIGHTAG | (uintptr_t)handle);
			} else {
				/* Append the CEL4RO31 diagnostics to errBuf. */
				char cel4ro_errBuf[128];
				snprintf(
					cel4ro_errBuf,
					sizeof(cel4ro_errBuf),
					" CEL4RO31 retcode: %d - %s",
					ro31ControlBlock->retcode,
					omr_cel4ro31_get_error_message(ro31ControlBlock->retcode));
				Trc_PRT_sl_open_shared_library_Event2(cel4ro_errBuf);
				strncat(errBuf, cel4ro_errBuf, sizeof(errBuf) - strlen(errBuf) - 1);
			}
			omr_cel4ro31_deinit(ro31InfoBlock);
		}

		if (initCEE_ENVFILE) {
			/* Clear _CEE_ENVFILE to avoid child processes issues with picking up the updated LIBPATH. */
			putenv("_CEE_ENVFILE=");
		}

		DMESSAGE(("omrsl_open_shared_library: Attempted to load 31-bit DLL Return Code: [%d] DLL handle: [%p]\n", ro31InfoBlock->ro31ControlBlock.retcode, handle))
	}
#endif /* defined(J9ZOS39064) */

	if (NULL == handle) {
		if (!isErrBufPopulated) {
			getDLError(portLibrary, errBuf, sizeof(errBuf));
			Trc_PRT_sl_open_shared_library_Event2(errBuf);
		}
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

#if defined(J9ZOS39064)
	if (OMR_ARE_ALL_BITS_SET(descriptor, OMRPORT_SL_ZOS_31BIT_TARGET_HIGHTAG)) {
		/* 31-bit target library */
		handle = (dllhandle *)(OMRPORT_SL_ZOS_31BIT_TARGET_MASK & descriptor);
		DMESSAGE(("\n Closing 31-bit library %x\n", descriptor))

		/* @TODO: Currently, CEL4RO31 does not support a dllfree option.  Need to update
		 * once support is provided.
		 */
		error = 0;
	} else
#endif /* defined(J9ZOS39064) */
	{
		DMESSAGE(("\nClose library %x\n", descriptor))
		handle = (dllhandle *)descriptor;
		error = dllfree(handle);
	}

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
	void *address = NULL;
	dllhandle *handle = NULL;

	Trc_PRT_sl_lookup_name_Entry(descriptor, name, argSignature);

#if defined(J9ZOS39064)
	if (OMR_ARE_ALL_BITS_SET(descriptor, OMRPORT_SL_ZOS_31BIT_TARGET_HIGHTAG)) {
		OMR_CEL4RO31_infoBlock *ro31InfoBlock = NULL;

		/* 31-bit target library */
		handle = (dllhandle *)(OMRPORT_SL_ZOS_31BIT_TARGET_MASK & descriptor);
		DMESSAGE(("omrsl_lookup_name: Attempt to query 31-bit DLL function [%s] from DLL handle: %p\n", name, handle))
		ro31InfoBlock = omr_cel4ro31_init(OMR_CEL4RO31_FLAG_QUERY_TARGET_FUNC, NULL, name, 0);
		if (NULL != ro31InfoBlock) {
			OMR_CEL4RO31_controlBlock *ro31ControlBlock = &(ro31InfoBlock->ro31ControlBlock);
			ro31ControlBlock->dllHandle = (uint32_t)handle;
			omr_cel4ro31_call(ro31InfoBlock);
			if (OMR_CEL4RO31_RETCODE_OK == ro31ControlBlock->retcode) {
				/* CEL4RO31 returns the function descriptor (FDCB) in the functionEntryPoint member */
				address = (void *)ro31ControlBlock->functionEntryPoint;
				address = (void *)(OMRPORT_SL_ZOS_31BIT_TARGET_HIGHTAG | (uintptr_t)address);
			}
			omr_cel4ro31_deinit(ro31InfoBlock);
			DMESSAGE(("omrsl_lookup_name: Attempted to 31-bit DLL function [%s] from DLL handle: %p - return code: [%d] Function Pointer: [%p]\n", name, handle, ro31InfoBlock->ro31ControlBlock.retcode, address))
		}
	} else
#endif /* defined(J9ZOS39064) */
	{
		handle = (dllhandle *)descriptor;
		address = (void *)dllqueryfn(handle, name);
	}

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

