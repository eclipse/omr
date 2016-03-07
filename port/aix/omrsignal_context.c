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

#include "omrport.h"
#include <unistd.h>
#include <sys/ucontext.h>
#include <sys/ldr.h>
#include "omrsignal_context.h"


/**
 * Note: mcontext_t is a typedeffed __jmpbuf structure
 */
void
fillInUnixSignalInfo(struct OMRPortLibrary *portLibrary, void *contextInfo, struct J9UnixSignalInfo *j9Info)
{
	j9Info->platformSignalInfo.context = (ucontext_t *)contextInfo;
	/* module info is filled on demand */
}

uint32_t
infoForSignal(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	int32_t *aSignalItem;

	switch (index) {
	case OMRPORT_SIG_SIGNAL_TYPE:
	case 0:
		*name = "J9Generic_Signal_Number";
		*value = &info->portLibrarySignalType;
		return OMRPORT_SIG_VALUE_32;
	case OMRPORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE:
	case 1:
		*name = "Signal_Number";
		aSignalItem = &info->sigInfo->si_signo;
		*value = aSignalItem;
		return OMRPORT_SIG_VALUE_32;
	case OMRPORT_SIG_SIGNAL_ERROR_VALUE:
	case 2:
		*name = "Error_Value";
		aSignalItem = &info->sigInfo->si_errno;
		*value = aSignalItem;
		return OMRPORT_SIG_VALUE_32;
	case OMRPORT_SIG_SIGNAL_CODE:
	case 3:
		*name = "Signal_Code";
		aSignalItem = &info->sigInfo->si_code;
		*value = aSignalItem;
		return OMRPORT_SIG_VALUE_32;
	case OMRPORT_SIG_SIGNAL_HANDLER:
	case 4:
		*name = "Handler1";
		*value = &info->handlerAddress;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 5:
		*name = "Handler2";
		*value = &info->handlerAddress2;
		return OMRPORT_SIG_VALUE_ADDRESS;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

uint32_t
infoForFPR(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	const char *n_fpr[NFPRS] = {
		"FPR0",
		"FPR1",
		"FPR2",
		"FPR3",
		"FPR4",
		"FPR5",
		"FPR6",
		"FPR7",
		"FPR8",
		"FPR9",
		"FPR10",
		"FPR11",
		"FPR12",
		"FPR13",
		"FPR14",
		"FPR15",
		"FPR16",
		"FPR17",
		"FPR18",
		"FPR19",
		"FPR20",
		"FPR21",
		"FPR22",
		"FPR23",
		"FPR24",
		"FPR25",
		"FPR26",
		"FPR27",
		"FPR28",
		"FPR29",
		"FPR30",
		"FPR31"
	};

	*name = "";

	if ((index >= 0) && (index < NFPRS)) {
		*name = n_fpr[index];
		*value = &(info->platformSignalInfo.context->uc_mcontext.jmp_context.fpr[index]);
		return OMRPORT_SIG_VALUE_FLOAT_64;
	} else {
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

uint32_t
infoForGPR(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	const char *n_gpr[NGPRS] = {
		"R0",
		"R1",
		"R2",
		"R3",
		"R4",
		"R5",
		"R6",
		"R7",
		"R8",
		"R9",
		"R10",
		"R11",
		"R12",
		"R13",
		"R14",
		"R15",
		"R16",
		"R17",
		"R18",
		"R19",
		"R20",
		"R21",
		"R22",
		"R23",
		"R24",
		"R25",
		"R26",
		"R27",
		"R28",
		"R29",
		"R30",
		"R31"
	};

	*name = "";

	if ((index >= 0) && (index < NGPRS)) {
		*name = n_gpr[index];
		*value = (void *)&(info->platformSignalInfo.context->uc_mcontext.jmp_context.gpr[index]);
		return OMRPORT_SIG_VALUE_ADDRESS;
	} else {
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

uint32_t
infoForControl(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	switch (index) {
	case OMRPORT_SIG_CONTROL_PC:
	case 0:
		*name = "IAR";
		*value = &info->platformSignalInfo.context->uc_mcontext.jmp_context.iar;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_LR:
	case 1:
		*name = "LR";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.jmp_context.lr;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_MSR:
	case 2:
		*name = "MSR";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.jmp_context.msr;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_CTR:
	case 3:
		*name = "CTR";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.jmp_context.ctr;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_CR:
	case 4:
		*name = "CR";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.jmp_context.cr;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_FPSCR:
	case 5:
		*name = "FPSCR";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.jmp_context.fpscr;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_XER:
	case 6:
		*name = "XER";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.jmp_context.xer;
		return OMRPORT_SIG_VALUE_ADDRESS;
#if !defined(PPC64)
	case 7:
		*name = "TID";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.jmp_context.tid;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_MQ:
	case 8:
		*name = "MQ";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.jmp_context.mq;
		return OMRPORT_SIG_VALUE_ADDRESS;
#endif
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

uint32_t
infoForModule(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	void *buffer = info->platformSignalInfo.ldInfo;
	void *iar = (void *)info->platformSignalInfo.context->uc_mcontext.jmp_context.iar;
	struct ld_info *ldInfo = NULL;
	int result;

	result = loadquery(L_GETINFO, buffer, sizeof(info->platformSignalInfo.ldInfo));

	if (result != -1) {
		struct ld_info *next, *cursor;

		/* try to find the offending module in the list */
		cursor = NULL;
		next = buffer;
		while (cursor != next) {
			cursor = next;
			if (iar >= cursor->ldinfo_textorg && iar < cursor->ldinfo_textorg + cursor->ldinfo_textsize) {
				ldInfo = cursor;
				break;
			}
			next = (struct ld_info *)((char *)cursor + cursor->ldinfo_next);
		}
	} else {
		/* TODO: tracepoint or something here? */
	}

	switch (index) {
	case 0:
		*name = "Module";
		if (ldInfo) {
			*value = (ldInfo->ldinfo_filename);
			return OMRPORT_SIG_VALUE_STRING;
		} else {
			return OMRPORT_SIG_VALUE_UNDEFINED;
		}
	case 1:
		*name = "Module_base_address";
		if (ldInfo) {
			*value = (void *)&(ldInfo->ldinfo_textorg);
			return OMRPORT_SIG_VALUE_ADDRESS;
		} else {
			return OMRPORT_SIG_VALUE_UNDEFINED;
		}
	default:
		*name = "";
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

