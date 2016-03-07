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
#include "omrsignal_context.h"


#ifdef PPC64
#define J9SIG_VALUE_UDATA OMRPORT_SIG_VALUE_64
#else
#define J9SIG_VALUE_UDATA OMRPORT_SIG_VALUE_32
#endif

#define NGPRS 32

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

#if defined(LINUXPPC64)
	const char *n_fpr[NGPRS] = {
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

	if ((index >= 0) && (index < NGPRS)) {
		*name = n_fpr[index];
		*value = &(info->platformSignalInfo.context->uc_mcontext.fp_regs[index]);
		return OMRPORT_SIG_VALUE_FLOAT_64;
	} else {
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
#endif

	return OMRPORT_SIG_VALUE_UNDEFINED;
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
		*value = &(info->platformSignalInfo.context->uc_mcontext.regs->gpr[index]);
		return OMRPORT_SIG_VALUE_ADDRESS;
	}

	return OMRPORT_SIG_VALUE_UNDEFINED;
}

uint32_t
infoForControl(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	switch (index) {
	case OMRPORT_SIG_CONTROL_PC:
	case 0:
		*name = "NIP";
		*value = &info->platformSignalInfo.context->uc_mcontext.regs->nip;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_MSR:
	case 1:
		*name = "MSR";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->msr;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 2:
		*name = "ORIG_GPR3";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->orig_gpr3;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_CTR:
	case 3:
		*name = "CTR";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->ctr;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_LR:
	case 4:
		*name = "LINK";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->link;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_XER:
	case 5:
		*name = "XER";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->xer;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_CR:
	case 6:
		*name = "CCR";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->ccr;
		return OMRPORT_SIG_VALUE_ADDRESS;
#if defined(PPC64)
	case 7:
		*name = "SOFTE";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->softe;
		return OMRPORT_SIG_VALUE_ADDRESS;
#else
	case OMRPORT_SIG_CONTROL_POWERPC_MQ:
	case 7:
		*name = "MQ";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->mq;
		return OMRPORT_SIG_VALUE_ADDRESS;
#endif
	case 8:
		*name = "TRAP";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->trap;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_DAR:
	case 9:
		*name = "DAR";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->dar;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_POWERPC_DSIR:
	case 10:
		*name = "dsisr";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->dsisr;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 11:
		*name = "RESULT";
		*value = (void *)&info->platformSignalInfo.context->uc_mcontext.regs->result;
		return OMRPORT_SIG_VALUE_ADDRESS;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}

}

uint32_t
infoForModule(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	void *address;
	Dl_info *dl_info = &(info->platformSignalInfo.dl_info);
	*name = "";

	int dl_result = dladdr((void *)info->platformSignalInfo.context->uc_mcontext.regs->nip, dl_info);

	switch (index) {
	case OMRPORT_SIG_MODULE_NAME:
	case 0:
		*name = "Module";
		if (dl_result) {
			*value = (void *)(dl_info->dli_fname);
			return OMRPORT_SIG_VALUE_STRING;
		}
		return OMRPORT_SIG_VALUE_UNDEFINED;
	case 1:
		*name = "Module_base_address";
		if (dl_result) {
			*value = (void *)&(dl_info->dli_fbase);
			return OMRPORT_SIG_VALUE_ADDRESS;
		}
		return OMRPORT_SIG_VALUE_UNDEFINED;
	case 2:
		*name = "Symbol";
		if (dl_result) {
			if (dl_info->dli_sname != NULL) {
				*value = (void *)(dl_info->dli_sname);
				return OMRPORT_SIG_VALUE_STRING;
			}
		}
		return OMRPORT_SIG_VALUE_UNDEFINED;
	case 3:
		*name = "Symbol_address";
		if (dl_result) {
			*value = (void *)&(dl_info->dli_saddr);
			return OMRPORT_SIG_VALUE_ADDRESS;
		}
		return OMRPORT_SIG_VALUE_UNDEFINED;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

