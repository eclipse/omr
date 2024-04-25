/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef omrcel4ro31_h
#define omrcel4ro31_h

#include "omrcomp.h"

/* Flags to control the behaviour of CEL4RO31. Used by RO31_CB->flags */
#define OMR_CEL4RO31_FLAG_LOAD_DLL               0x80000000
#define OMR_CEL4RO31_FLAG_QUERY_TARGET_FUNC      0x40000000
#define OMR_CEL4RO31_FLAG_EXECUTE_TARGET_FUNC    0x20000000
#define OMR_CEL4RO31_FLAG_ALL_LOAD_QUERY_EXECUTE (OMR_CEL4RO31_FLAG_LOAD_DLL | OMR_CEL4RO31_FLAG_QUERY_TARGET_FUNC | OMR_CEL4RO31_FLAG_EXECUTE_TARGET_FUNC)

/* Return codes for CEL4RO31 */
#define OMR_CEL4RO31_RETCODE_OK                                   0x0
#define OMR_CEL4RO31_RETCODE_ERROR_MULTITHREAD_INVOCATION         0x1
#define OMR_CEL4RO31_RETCODE_ERROR_STORAGE_ISSUE                  0x2
#define OMR_CEL4RO31_RETCODE_ERROR_FAILED_AMODE31_ENV             0x3
#define OMR_CEL4RO31_RETCODE_ERROR_FAILED_LOAD_TARGET_DLL         0x4
#define OMR_CEL4RO31_RETCODE_ERROR_FAILED_QUERY_TARGET_FUNC       0x5
#define OMR_CEL4RO31_RETCODE_ERROR_LE_RUNTIME_SUPPORT_NOT_FOUND   0x6

/* Return codes for CELQGIPB */
#define CELQGIPB_RETCODE_INITIAL                0xFFFFFFFF
#define CELQGIPB_RETCODE_OK                     0x0
#define CELQGIPB_RETCODE_FAILED_ALLOC           0x1
#define CELQGIPB_RETCODE_DEPTH_EXCEEDED         0x2
#define CELQGIPB_RETCODE_OTHER_THREAD_SWITCHING 0x3

/* Flags for OMR_CEL4RO31 buffer status */
#define OMR_CEL4RO31_INFOBLOCK_FLAGS_MALLOC31    0x1

/* Fixed length Control Block structure CEL4RO31 */
typedef struct OMR_CEL4RO31_controlBlock {
	uint32_t version;               /**< (Input) A integer which contains the version of RO31_INFO. */
	uint32_t length;                /**< (Input) A integer which contains the total length of RO31_INFO. */
	uint32_t flags;                 /**< (Input) Flags to control the behavior of CEL4RO31. */
	uint32_t moduleOffset;          /**< (Input) Offset to RO31_module section from start of RO31_CB. Req'd for dll load flag. */
	uint32_t functionOffset;        /**< (Input) Offset to RO31_function section from start of RO31_CB. Req'd for dll query flag. */
	uint32_t argumentsOffset;       /**< (Input) Offset to outgoing arguments section from start of RO31_CB. Req'd for function execution flag. */
	uint32_t dllHandle;             /**< DLL handle of target program (Input) DLL handle if dll query flag. (Output) DLL handle if dll load flag. */
	uint32_t functionDescriptor;    /**< Function descriptor of target function (Input) Func Desc if function execution flag. (Output) Func Desc if dll query flag. */
	uint32_t gpr15ReturnValue;      /**< (Output) Return GPR buffer containing 32-bit GPR15 value when target program returned after execution. */
	uint32_t gpr0ReturnValue;       /**< (Output) Return GPR buffer containing 32-bit GPR0 value when target program returned after execution. */
	uint32_t gpr1ReturnValue;       /**< (Output) Return GPR buffer containing 32-bit GPR1 value when target program returned after execution. */
	uint32_t gpr2ReturnValue;       /**< (Output) Return GPR buffer containing 32-bit GPR2 value when target program returned after execution. */
	uint32_t gpr3ReturnValue;       /**< (Output) Return GPR buffer containing 32-bit GPR3 value when target program returned after execution. */
	int32_t retcode;                /**< (Output) Return code of CEL4RO31. */
} OMR_CEL4RO31_controlBlock;

/* Internal struct to map module name definition in control block */
typedef struct OMR_CEL4RO31_module {
	uint32_t length;        /**< Length of the module name */
	char moduleName[1];     /**< Pointer to variable length module name. */
} OMR_CEL4RO31_module;

/* Internal struct to map function name definition in control block */
typedef struct OMR_CEL4RO31_function {
	uint32_t length;        /**< Length of the function name */
	char functionName[1];   /**< Pointer to variable length function name. */
} OMR_CEL4RO31_function;

/* Internal struct to track the RO31_CB and related variable length structures */
typedef struct OMR_CEL4RO31_infoBlock {
	uint32_t flags;                                  /**< Miscellaneous flags denoting state of buffer. */
	OMR_CEL4RO31_module *ro31Module;                 /**< Pointer to the start of the module section of the control block. */
	OMR_CEL4RO31_function *ro31Function;             /**< Pointer to the start of the function section of the control block. */
	void *ro31_args;                                 /**< Pointer to the start of the outgoing arguments section of the control block. */
	OMR_CEL4RO31_controlBlock ro31ControlBlock;      /**< Pointer to the control block for CEL4RO31. */
} OMR_CEL4RO31_infoBlock;

/**
 * A helper routine to allocate the CEL4RO31 control blocks and related structures
 * for module, function and arguments.
 * @param[in] flags The OMR_CEL4RO31_FLAG_* flags for load library, query and execute target functions.
 * @param[in] moduleName The name of the target library to load.
 * @param[in] functionName The name of the target function to lookup.
 * @param[in] argsLength The length of the outgoing parameters, in bytes.
 *
 * @return An initialized CEL4RO31 control block, NULL if not successful.
 */
OMR_CEL4RO31_infoBlock *
omr_cel4ro31_init(uint32_t flags, const char* moduleName, const char* functionName, uint32_t argsLength);

/**
 * A Helper routine to deallocate the CEL4RO31 control blocks and related structures
 * for module, function and arguments.
 * @param[in] infoBlock A control block associated with this call for deallocation.
 */
void
omr_cel4ro31_deinit(OMR_CEL4RO31_infoBlock *infoBlock);

/**
 * A helper routine to make the CEL4RO31 call.
 * @param[in] infoBlock A completed control block for target invocation.
 *
 * @note The success/failure of this call invocation is indicated via the retcode member of the control block.
 */
void
omr_cel4ro31_call(OMR_CEL4RO31_infoBlock *infoBlock);

/**
 * A helper routine to confirm LE support of CEL4RO31
 * @return whether CEL4RO31 runtime support is available.
 */
BOOLEAN
omr_cel4ro31_is_supported(void);

/**
 * A helper routine to return an error message associated with the CEL4RO31 return code.
 * @param[in] retCode A CEL4RO31 return code.
 *
 * @return A message describing the conditions of the given error.
 */
const char *
omr_cel4ro31_get_error_message(int retCode);

#endif /* omrcel4ro31_h */
