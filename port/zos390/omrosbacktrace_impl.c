/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

/**
 * @file
 * @ingroup Port
 * @brief process introspection support
 */

#include "omrport.h"
#include "omrportpriv.h"
#include "omrsignal_context.h"
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#ifndef J9ZOS39064
#include "omrceetbck.h"
#endif

#include "omrintrospect.h"


int
getEntryName(uint8_t *entry_point, char **name)
{
	XPLINK_Routine_entry *routine_entry;
	PPA1_Version3 *ppa;
	char *namePtr = NULL;
	uint16_t *nameLenPtr = NULL;
	uint16_t  nameLen = 0;
	uint32_t optional_area_length = 0;

	if (entry_point == NULL) {
		*name = NULL;
		return 0;
	}
	/* get the address of the routine entry block */
	routine_entry = (XPLINK_Routine_entry *)(entry_point - sizeof(XPLINK_Routine_entry));

	/* get the address of the XLINK PPA1 */
	ppa = (PPA1_Version3 *)((uint8_t *)routine_entry + routine_entry->ppa1_offset);

	/* check of XPLINK PPA (PPA version 3 is identified by 0x02) */
	if (ppa->version == 0x02) {
		if (ppa->flags4 & NAME_LENGTH_AND_NAME) {
			/* this PPA has the entry name and length, so calculate its position */
			/* by working out the length of the optional fields. */

			/* to calculate the length of the optional fields, we must */
			/* ascertain their presence via the flags fields. */

			if (ppa->flags3 & STATE_VARIABLE_LOCATOR) {
				optional_area_length += 4;
			}

			if (ppa->flags3 & ARGUMENT_AREA_LENGTH) {
				optional_area_length += 4;
			}

			if (ppa->flags3 & FP_REGISTER || ppa->flags3 & AR_SAVED) {
				optional_area_length += 4;
			}

			if (ppa->flags3 & PPA1_WORD_FLAG) {
				optional_area_length += 4;
			}

			if (ppa->flags3 & BDI_OFFSET) {
				optional_area_length += 4;
			}

			if (ppa->flags3 & INTERFACE_MAPPING) {
				optional_area_length += 4;
			}

			if (ppa->flags3 & JAVA_METHOD_LOCATOR) {
				optional_area_length += 8;
			}

			nameLenPtr = (uint16_t *)((uint8_t *)ppa + sizeof(PPA1_Version3) + optional_area_length);
			nameLen = *nameLenPtr;
			namePtr = (uint8_t *)nameLenPtr + sizeof(nameLen);

		}
	}

	*name = namePtr;
	return nameLen;
}

/* This function constructs a backtrace from a CPU context. Generally there are only one or two
 * values in the context that are actually used to construct the stack but these vary by platform
 * so aren't detailed here. If no heap is specified then this function will use malloc to allocate
 * the memory necessary for the stack frame structures which must be freed by the caller.
 *
 * @param portLbirary a pointer to an initialized port library
 * @param threadInfo the thread structure we want  to attach the backtrace to. Must not be NULL.
 * @param heap a heap from which to allocate any necessary memory. If NULL malloc is used instead.
 * @param signalInfo a platform signal context. If not null the context held in threadInfo is replaced
 *  	  with the signal context before the backtrace is generated.
 *
 * @return the number of frames in the backtrace.
 */
uintptr_t
omrintrospect_backtrace_thread_raw(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap, void *signalInfo)
{
	uintptr_t frameNumber = 0;
	J9PlatformStackFrame **nextFrame;
	OMRUnixSignalInfo *sigInfo = (OMRUnixSignalInfo *)signalInfo;
	const char *regName = "";
	void **faultingAddress = 0;

	/* Main walking parameters */
	void *dsaptr = NULL;
	void *caaptr = NULL;
	void *callers_dsaptr;

#ifndef J9ZOS39064
	_INT4 dsa_format = 1; /* XP link */
	_INT4 callers_dsa_format;
	_INT4 entry_address;

	/* InOut parameters to CEETBCK */
	_INT4 call_instruction_address = 0;
	_INT4 program_unit_name_length = MAX_NAME;
	_INT4 entry_name_length = MAX_NAME;
	_INT4 statement_id_length = MAX_NAME;
	char entry_name[MAX_NAME + 1];
	char program_unit_name[MAX_NAME + 1];
	char statement_id[MAX_NAME + 1];

	/* Output parameters to CEETBCK */
	_INT4 program_unit_address = 0;
	_INT4 callers_call_instruction_address = 0;
	_INT4 member_id;
	void *cibptr;
	_INT4 main_program;

	struct _FEEDBACK fc;

	/* initialize the return code and loop control prior to making the calls */
	fc.tok_sev = 0;

#else
	/* There is no 64-bit ceetbck */
	int dsa_format = __EDCWCCWI_DOWN;
	int callers_dsa_format;
	int entry_address;
#endif

	if (threadInfo == NULL || (threadInfo->context == NULL && sigInfo == NULL)) {
		return 0;
	}

	/* if we've been passed a port library wrapped signal, then extract info from there */
	if (sigInfo != NULL) {
		threadInfo->context = sigInfo->platformSignalInfo.context;

		/* get the faulting address so we can discard frames that are part of the signal handling */
		infoForControl(portLibrary, sigInfo, 0, &regName, (void **)&faultingAddress);
	}

	/* check that CAA and DSA are available. We can't take these values from the signal as we have no way
	 * of knowing what the DSA format was for the code that received the signal
	 */
	if (threadInfo->dsa) {
		/* check if there's dsa/caa information included in the thread */
		dsaptr = threadInfo->dsa;
		dsa_format = threadInfo->dsa_format;
	} else {
		/* if we don't have the values set then we walk from this function */
		dsaptr = (void *)getdsa();

		/* have to skip this first frame as we don't want to return a DSA entry for this function as it'll be
		 * invalid once it returns
		 */
		dsaptr = __dsa_prev(dsaptr, __EDCWCCWI_LOGICAL, (int) dsa_format, NULL, NULL, (int *)&callers_dsa_format, NULL, NULL);
		dsa_format = callers_dsa_format;

		if (dsaptr == NULL) {
			/* can't walk the thread stack without this information */
			return 0;
		}
	}

#ifndef J9ZOS39064
	if (threadInfo->caa) {
		caaptr = threadInfo->caa;
	} else {
		caaptr = (void *)_gtca();
		if (caaptr == NULL) {
			return 0;
		}
	}
#endif

	callers_dsaptr = (void *)0x1;
	nextFrame = &threadInfo->callstack;

	/* Repeatedly call the z/OS CEETCBK service to get the backtrace */
	for (frameNumber = 0; callers_dsaptr != NULL; frameNumber++) {
#ifndef J9ZOS39064
		entry_name_length = 0;
		program_unit_name_length = 0;
		statement_id_length = 0;

		/* CAA field CEECAA_STACKDIRECTION holds stack direction... is this for the current frame or initial thread? */
		ceetbck(&dsaptr,							/* in */
				&dsa_format,						/* inout */
				&caaptr,							/* in */
				&member_id,							/* out */
				&program_unit_name[0],				/* out */
				&program_unit_name_length,			/* inout */
				&program_unit_address,				/* out */
				&call_instruction_address,			/* inout */
				&entry_name[0],						/* out */
				&entry_name_length,					/* inout */
				&entry_address,						/* out */
				&callers_call_instruction_address,	/* out */
				&callers_dsaptr,					/* out */
				&callers_dsa_format,				/* out */
				statement_id,						/* out */
				&statement_id_length,				/* inout */
				&cibptr,							/* out */
				&main_program,						/* out */
				&fc);								/* out */

		if (fc.tok_sev != 0) {
			break;
		}
#else
		errno = 0;
		callers_dsaptr = __dsa_prev(dsaptr, __EDCWCCWI_PHYSICAL, dsa_format, NULL, NULL, &callers_dsa_format, NULL, NULL);
		if (callers_dsaptr == NULL || errno) {
			break;
		}
#endif

		if (heap == NULL) {
			*nextFrame = portLibrary->mem_allocate_memory(portLibrary, sizeof(J9PlatformStackFrame), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		} else {
			*nextFrame = portLibrary->heap_allocate(portLibrary, heap, sizeof(J9PlatformStackFrame));
		}

		if (*nextFrame == NULL) {
			if (!threadInfo->error) {
				threadInfo->error = ALLOCATION_FAILURE;
			}

			break;
		}

		(*nextFrame)->parent_frame = NULL;
		(*nextFrame)->symbol = NULL;
#ifndef J9ZOS39064
		(*nextFrame)->instruction_pointer = (uintptr_t)call_instruction_address;
		(*nextFrame)->register1 = (uintptr_t)caaptr;
#else
		(*nextFrame)->instruction_pointer = 0;
#endif
		(*nextFrame)->register2 = dsa_format;
		(*nextFrame)->base_pointer = (uintptr_t)dsaptr;

		nextFrame = &(*nextFrame)->parent_frame;

		/* set up for the next call */
		dsaptr = callers_dsaptr;
#ifndef J9ZOS39064
		call_instruction_address = callers_call_instruction_address;
#endif
		dsa_format = callers_dsa_format;
	}

	return frameNumber;
}

/* This function takes a thread structure already populated with a backtrace by omrintrospect_backtrace_thread
 * and looks up the symbols for the frames. The format of the string generated is:
 * 		symbol_name (statement_id instruction_pointer [module+offset])
 * If it isn't possible to determine any of the items in the string then they are omitted. If no heap is specified
 * then this function will use malloc to allocate the memory necessary for the symbols which must be freed by the caller.
 *
 * @param portLbirary a pointer to an initialized port library
 * @param threadInfo a thread structure populated with a backtrace
 * @param heap a heap from which to allocate any necessary memory. If NULL malloc is used instead.
 * @param options controls how much effort is expended trying to resolve symbols
 *
 * @return the number of frames for which a symbol was constructed.
 */
uintptr_t
omrintrospect_backtrace_symbols_raw(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap, uint32_t options)
{
	uintptr_t frameNumber = 0;
	J9PlatformStackFrame *frame;
	int i;

	/* Main walking parameters */
	void *dsaptr = NULL;
	void *caaptr = NULL;
	void *callers_dsaptr;

#ifndef J9ZOS39064
	_INT4 dsa_format = 1; /* XP link */
	_INT4 callers_dsa_format;
	_INT4 entry_address;

	/* InOut parameters to CEETBCK */
	_INT4 call_instruction_address = 0;
	_INT4 program_unit_name_length = MAX_NAME;
	_INT4 entry_name_length = MAX_NAME;
	_INT4 statement_id_length = MAX_NAME;
	char entry_name[MAX_NAME + 1];
	char program_unit_name[MAX_NAME + 1];
	char statement_id[MAX_NAME + 1];

	/* Output parameters to CEETBCK */
	_INT4 program_unit_address = 0;
	_INT4 callers_call_instruction_address = 0;
	_INT4 member_id;
	void *cibptr;
	_INT4 main_program;

	struct _FEEDBACK fc;

	/* initialize the return code and loop control prior to making the calls */
	fc.tok_sev = 0;

#else
	/* There is no 64-bit ceetbck */
	int program_unit_name_length = MAX_NAME;
	int entry_name_length = MAX_NAME;
	int statement_id_length = MAX_NAME;
	void *program_unit_address = 0;

	char statement_id[MAX_NAME + 1];
	char entry_name[MAX_NAME + 1];
	char program_unit_name[MAX_NAME + 1];
	int dsa_format = __EDCWCCWI_DOWN;
	int callers_dsa_format;
	void *entry_address;
#endif

	if (threadInfo == NULL || threadInfo->callstack == NULL) {
		return 0;
	}

	/* work through the address_array and look up the symbol for each address */
	for (i = 0, frame = threadInfo->callstack; frame != NULL; frame = frame->parent_frame, i++) {
		char *module_name = "";
		char *symbol_name = "";
		char *line_name = "";
		int symbol_offset = 0;
		int module_offset = 0;
		int length;
#ifndef J9ZOS39064
		_INT4 format = frame->register2;
#endif
		char output_buf[512];
		char *cursor = output_buf;

		entry_name_length = MAX_NAME;
		program_unit_name_length = MAX_NAME;
		statement_id_length = MAX_NAME;

#ifndef J9ZOS39064
		ceetbck((_POINTER *)&frame->base_pointer,				/* in */
				&format,							/* inout */
				(_POINTER *)&frame->register1,					/* in */
				&member_id,							/* out */
				&program_unit_name[0],				/* out */
				&program_unit_name_length,			/* inout */
				&program_unit_address,				/* out */
				(_INT4 *)&frame->instruction_pointer,/* inout */
				&entry_name[0],						/* out */
				&entry_name_length,					/* inout */
				&entry_address,						/* out */
				&callers_call_instruction_address,	/* out */
				&callers_dsaptr,					/* out */
				&callers_dsa_format,				/* out */
				statement_id,						/* out */
				&statement_id_length,				/* inout */
				&cibptr,							/* out */
				&main_program,						/* out */
				&fc);								/* out */

#else
		entry_name_length = 0;
		program_unit_name_length = 0;
		statement_id_length = 0;

		entry_address = __ep_find((void *)frame->base_pointer, frame->register2, NULL);
#endif

		/* successful call */
		/* for XPLINK, call getEntryName (it works better) */
#ifndef J9ZOS39064
		if (frame->register2 == 1) {
#else
		if (frame->register2 == __EDCWCCWI_DOWN) {
#endif
			/* null terminate the ceetbck name */
			entry_name[entry_name_length] = 0;

			/* try getEntryName */
			entry_name_length = getEntryName((uint8_t *)entry_address, &symbol_name);
			if (entry_name_length != 0 && entry_name_length <= MAX_NAME) {
				/* copy and null terminate */
				strncpy(entry_name, symbol_name, entry_name_length);
				entry_name[entry_name_length] = 0;
			} else {
				entry_name_length = 0;
			}
		} else {
			/* some upstack names seem to translate badly, e.g. C?????????0x0000ED28 [+0x8f8])
			 * We just omit the symbol instead
			 */
			entry_name[0] = 0;
		}

		if (entry_name_length == 0) {
			symbol_name = "";
		} else {
			/* we have a name that needs converting */
			symbol_name = entry_name;
			if (__etoa(symbol_name) != entry_name_length) {
				symbol_name = "";
				entry_name_length = 0;
			}
		}

		if (program_unit_name_length == 0) {
			module_name = "";
		} else {
			module_name = program_unit_name;
			program_unit_name[program_unit_name_length] = 0;
			if (__etoa(module_name) != program_unit_name_length) {
				module_name = "";
				program_unit_name_length = 0;
			} else {
				/* ensure we strip leading paths */
				module_name = strrchr(module_name, '/');
				if (module_name == NULL) {
					module_name = program_unit_name;
				}
			}
		}

		/* the statement ID's only seem to be coherent for MVS modules, in USS code it looks like random data */

		/* if we have symbol information then present offset into symbol instead of offset into module */
		if (entry_name_length > 0) {
			symbol_offset = frame->instruction_pointer - (uintptr_t)entry_address;
		}

		if (program_unit_address != 0) {
			module_offset = frame->instruction_pointer - (uintptr_t)program_unit_address;
		}

		/* symbol_name+offset (id, instruction_pointer [module+offset]) */
		if (symbol_name[0] != '\0') {
			cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), "%s", symbol_name);
			cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), "+0x%x ", symbol_offset);
		}
		*(cursor++) = '(';
		if (statement_id[0] != '\0') {
			cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), "%.*s, ", statement_id_length, statement_id);
		}
		cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), "0x%p", frame->instruction_pointer);
		if (module_name[0] != '\0') {
			cursor += omrstr_printf(portLibrary, cursor, sizeof(output_buf) - (cursor - output_buf), " [%s+0x%x]", module_name, module_offset);
		}
		*(cursor++) = ')';
		*cursor = 0;

		length = (cursor - output_buf) + 1;
		if (heap == NULL) {
			frame->symbol = portLibrary->mem_allocate_memory(portLibrary, length, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		} else {
			frame->symbol = portLibrary->heap_allocate(portLibrary, heap, length);
		}

		if (frame->symbol != NULL) {
			strncpy(frame->symbol, output_buf, length);
		} else {
			frame->symbol = NULL;
			if (!threadInfo->error) {
				threadInfo->error = ALLOCATION_FAILURE;
			}
			i--;
		}
	}

	return i;
}
