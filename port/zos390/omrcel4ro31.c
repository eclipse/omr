/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#include "omrcel4ro31.h"

#include <unistd.h> /* for _a2e_l */
#include <string.h> /* for strncpy */
#include <stdlib.h> /* for malloc31 */


/* Function descriptor of CEL4RO31 runtime call from GTCA control block */
typedef void cel4ro31_cwi_func(void *);
#define CEL4RO31_FNPTR ((cel4ro31_cwi_func *)((char *)(*(int *)(((char *)__gtca())+1096))+8))

/* Function descriptor of CELQGIPB runtime call from GTCA control block */
typedef void celqgipb_cwi_func(uint32_t *, OMR_CEL4RO31_infoBlock **, uint32_t *);
#define CELQGIPB_FNPTR ((celqgipb_cwi_func *)((char *)(*(int *)(((char *)__gtca())+1096))+48))

OMR_CEL4RO31_infoBlock *
omr_cel4ro31_init(uint32_t flags, const char *moduleName, const char *functionName, uint32_t argsLength)
{
	/* Interprocedural buffer (IPB) contains the below-the-bar control blocks and outgoing
	 * arguments to facilitate the cross-AMODE call.
	 */
	uint32_t IPBlength = 0;
	uint32_t IPBretcode = CELQGIPB_RETCODE_INITIAL;
	uint32_t moduleNameLength = 0;
	uint32_t functionNameLength = 0;

	OMR_CEL4RO31_infoBlock *infoBlock = NULL;
	OMR_CEL4RO31_controlBlock *controlBlock = NULL;
	OMR_CEL4RO31_module *moduleBlock = NULL;
	OMR_CEL4RO31_function *functionBlock = NULL;

	/* For DLL load operations, we expect a module name specified. */
	if (OMR_ARE_ANY_BITS_SET(flags, OMR_CEL4RO31_FLAG_LOAD_DLL)) {
		moduleNameLength = strlen(moduleName);
	}

	 /* For DLL query operations, we expect a function name specified. */
	if (OMR_ARE_ANY_BITS_SET(flags, OMR_CEL4RO31_FLAG_QUERY_TARGET_FUNC)) {
		functionNameLength = strlen(functionName);
	}

	/* Request LE-managed IPB in below-the-bar storage. */
	IPBlength = sizeof(OMR_CEL4RO31_infoBlock) + moduleNameLength + functionNameLength + argsLength +
	            sizeof(moduleBlock->length) + sizeof(functionBlock->length);
	if (NULL != CELQGIPB_FNPTR) {
		CELQGIPB_FNPTR(&IPBlength, &infoBlock, &IPBretcode);
	}

	if ((CELQGIPB_RETCODE_OK != IPBretcode)) {
		/* IPB unavailable or request failed. Attempt to allocate below-the-bar storage ourselves. */
		infoBlock = (OMR_CEL4RO31_infoBlock *)__malloc31(IPBlength);
		if (NULL == infoBlock) {
			return NULL;
		}
		infoBlock->flags = OMR_CEL4RO31_INFOBLOCK_FLAGS_MALLOC31;
	} else {
		infoBlock->flags = 0;
	}

	controlBlock = &(infoBlock->ro31ControlBlock);

	/* Initialize the RO31 control block members and calculate the various offsets. */
	controlBlock->version = 1;
	controlBlock->flags = flags;
	controlBlock->moduleOffset = sizeof(OMR_CEL4RO31_controlBlock);
	if (0 == moduleNameLength) {
		controlBlock->functionOffset = controlBlock->moduleOffset;
	} else {
		controlBlock->functionOffset = controlBlock->moduleOffset + moduleNameLength + sizeof(moduleBlock->length);
	}
	controlBlock->retcode = 0;
	/* Control block length should exclude the infoBlock's flags and reference pointers. */
	controlBlock->length = IPBlength - (sizeof(OMR_CEL4RO31_infoBlock) - sizeof(OMR_CEL4RO31_controlBlock));

	/* For execute target function operations, we need to ensure args reference is set
	 * up appropriately. If no arguments, argumentsOffset needs to be 0.
	 */
	if (0 == argsLength) {
		controlBlock->argumentsOffset = 0;
	} else if (0 == functionNameLength) {
		controlBlock->argumentsOffset = controlBlock->functionOffset;
	} else {
		controlBlock->argumentsOffset = controlBlock->functionOffset + functionNameLength + sizeof(functionBlock->length);
	}

	/* For DLL load operations, we expect a module name specified. */
	moduleBlock = (OMR_CEL4RO31_module *)((char *)(controlBlock) + controlBlock->moduleOffset);
	infoBlock->ro31Module = moduleBlock;
	if (OMR_ARE_ANY_BITS_SET(flags, OMR_CEL4RO31_FLAG_LOAD_DLL)) {
		moduleBlock->length = moduleNameLength;
		strncpy(moduleBlock->moduleName, moduleName, moduleNameLength);
		__a2e_l((char *)moduleBlock->moduleName, moduleNameLength);
	}

	/* For DLL query operations, we expect a function name specified. */
	functionBlock = (OMR_CEL4RO31_function *)((char *)(controlBlock) + controlBlock->functionOffset);
	infoBlock->ro31Function = functionBlock;
	if (OMR_ARE_ANY_BITS_SET(flags, OMR_CEL4RO31_FLAG_QUERY_TARGET_FUNC)) {
		functionBlock->length = functionNameLength;
		strncpy(functionBlock->functionName, functionName, functionNameLength);
		__a2e_l((char *)functionBlock->functionName, functionNameLength);
	}

	/* For execute target function operations, we need to ensure args is set
	 * up appropriately. If no arguments, argumentsOffset needs to be 0.
	 */
	if (OMR_ARE_ANY_BITS_SET(flags, OMR_CEL4RO31_FLAG_EXECUTE_TARGET_FUNC)) {
		infoBlock->ro31_args = (void *)((char *)(controlBlock) + controlBlock->argumentsOffset);
	}

	return infoBlock;
}

void
omr_cel4ro31_deinit(OMR_CEL4RO31_infoBlock *infoBlock)
{
	if (NULL != infoBlock) {
		/* LE-managed IPB will be automatically re-allocated for subsqeuent calls at same level.
		 * Free any storage we allocated ourselves due to IPB request failure.
		 */
		if (OMR_ARE_ANY_BITS_SET(infoBlock->flags, OMR_CEL4RO31_INFOBLOCK_FLAGS_MALLOC31)) {
			free(infoBlock);
		}
	}
}

void
omr_cel4ro31_call(OMR_CEL4RO31_infoBlock *infoBlock)
{
	/* The CEL4RO31 function may not be available if the LE PTF is not installed. */
	if (NULL == CEL4RO31_FNPTR) {
		infoBlock->ro31ControlBlock.retcode = OMR_CEL4RO31_RETCODE_ERROR_LE_RUNTIME_SUPPORT_NOT_FOUND;
		return;
	}

	CEL4RO31_FNPTR(&(infoBlock->ro31ControlBlock));
}

BOOLEAN
omr_cel4ro31_is_supported(void)
{
	return (NULL != CEL4RO31_FNPTR) && (NULL != CELQGIPB_FNPTR);
}

const char*
omr_cel4ro31_get_error_message(int retCode)
{
	const char* errorMessage;
	switch(retCode) {
	case OMR_CEL4RO31_RETCODE_OK:
		errorMessage = "No errors detected.";
		break;
	case OMR_CEL4RO31_RETCODE_ERROR_MULTITHREAD_INVOCATION:
		errorMessage = "Multi-threaded invocation not supported.";
		break;
	case OMR_CEL4RO31_RETCODE_ERROR_STORAGE_ISSUE:
		errorMessage = "Storage issue detected in CEL4RO31.";
		break;
	case OMR_CEL4RO31_RETCODE_ERROR_FAILED_AMODE31_ENV:
		errorMessage = "Failed to initialize AMODE31 environment.";
		break;
	case OMR_CEL4RO31_RETCODE_ERROR_FAILED_LOAD_TARGET_DLL:
		errorMessage = "DLL module not found.";
		break;
	case OMR_CEL4RO31_RETCODE_ERROR_FAILED_QUERY_TARGET_FUNC:
		errorMessage = "Target function not found.";
		break;
	case OMR_CEL4RO31_RETCODE_ERROR_LE_RUNTIME_SUPPORT_NOT_FOUND:
		errorMessage = "CEL4RO31 runtime support not found.";
		break;
	default:
		errorMessage = "Unexpected return code.";
		break;
	}
	return errorMessage;
}
