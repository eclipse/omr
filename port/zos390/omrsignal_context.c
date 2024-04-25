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

#include "omrport.h"
#include "omrportpriv.h"
#include <strings.h>
#include <unistd.h>
#include "omrsignal_context.h"
#include "omrceeocb.h"

#ifndef J9ZOS39064
#include "omrceetbck.h"
#endif

#if defined(OMR_ENV_DATA64)
#include <__le_api.h>
#else
#include "omr__le_api.h"
#endif

#if 0
#define SIG_DEBUG
#endif


/* Returns TRUE if the configuration supports resuming execution from a signal handler,
 * FALSE otherwise
 */
BOOLEAN
checkIfResumableTrapsSupported(struct OMRPortLibrary *portLibrary)
{
	/* Note that omrceeocb.h was introduced solely to provide support for this function */

	struct ceeocb_trap_sub_options *trap_opts;
	int trap_on;
	BOOLEAN rc = FALSE;
	char infoString[512];

#if defined(SIG_DEBUG)
	portLibrary->tty_printf(portLibrary, "%.8s\n", ceeedb()->ceeedbeye);
	portLibrary->tty_printf(portLibrary, "%.8s\n", ceeocb()->ceeocb_eyecatcher);
	portLibrary->tty_printf(portLibrary, "__trap=%d\n", __trap);
#endif

	trap_on = ceeocb()->ceeocb_opt[__trap].ceeocb_opt_on;
	trap_opts = (struct ceeocb_trap_sub_options *)((char *)ceeocb() + ceeocb()->ceeocb_opt[__trap].ceeocb_opt_subopts_offset);

#if defined(SIG_DEBUG)
	portLibrary->tty_printf(portLibrary, "trap(on)=%01x\n", trap_on);
	portLibrary->tty_printf(portLibrary, "spie=%01x\n", trap_opts->ceeocb_trap_spie);
#endif

	/* check that we have TRAP(ON,SPIE) */
	if ((0 != trap_on) && (0 != trap_opts->ceeocb_trap_spie)) {
		/* we have TRAP(ON,SPIE), we can continue execution from a signal handler */
		rc = TRUE;
	}

	return rc;
}


void
fillInUnixSignalInfo(struct OMRPortLibrary *portLibrary, void *contextInfo, struct OMRUnixSignalInfo *signalInfo)
{
	/* __mcontext_t_ is defined in port/zos390/edcwccwi.h and appears to overlay the ucontext_t structure */
	signalInfo->platformSignalInfo.context = (__mcontext_t_ *) contextInfo;
	/* module info is filled on demand */
}

void
fillInJumpInfo(struct OMRPortLibrary *portLibrary, void *contextInfo, void  *jumpInfo)
{
	int i = 0;
	__mcontext_t_ * sigContextInfo  = (__mcontext_t_ *)contextInfo;
	struct __jumpinfo *farJumpInfo  = (struct __jumpinfo *)jumpInfo;

	/* zero out the jump buffer*/
	{
		struct __jumpinfo_vr_ext *const jpi = farJumpInfo->__ji_vr_ext;

		bzero(farJumpInfo, sizeof(struct __jumpinfo));

		if (portLibrary->portGlobals->vectorRegsSupportOn) {
			farJumpInfo->__ji_vr_ext = jpi;
		}
	}

	/* If we are using the vector registers on zSphinx, do not attempt
	 * to restore the values of the FPRs (see JAZZ 81821)
	 */
	if (portLibrary->portGlobals->vectorRegsSupportOn) {
		farJumpInfo->__ji_fl_fp4  = 0;
		farJumpInfo->__ji_fl_fp16 = 0;
	} else {
		farJumpInfo->__ji_fl_fp4  = sigContextInfo->__mc_fl_fp4;
		farJumpInfo->__ji_fl_fp16 = sigContextInfo->__mc_fl_fp16;
	}

	farJumpInfo->__ji_fl_fpc	= sigContextInfo->__mc_fl_fpc	;
	farJumpInfo->__ji_fl_res1a = 0 /*sigContextInfo->__mc_fl_res1a */ ;

#if !defined(OMR_ENV_DATA64)
	farJumpInfo->__ji_fl_hr	 = sigContextInfo->__mc_fl_hr	 ;
#endif
	farJumpInfo->__ji_fl_res2  = 0 /*sigContextInfo->__mc_fl_res2 */  ;
	farJumpInfo->__ji_fl_exp	= sigContextInfo->__mc_fl_exp	;
	farJumpInfo->__ji_fl_res2a = 0 /*sigContextInfo->__mc_fl_res2a */ ;
	farJumpInfo->__ji_vr_ext = sigContextInfo->__mc_vr_ext;

	/* For the time being, LE requires this field to be set to zero */
	if (portLibrary->portGlobals->vectorRegsSupportOn) {
		farJumpInfo->__ji_vr_ext->__ji_ve_version = 0;
	}

	for (i = 0; i < 16; i++) {
		farJumpInfo->__ji_gr[i] = sigContextInfo->__mc_gr[i];
	}
#if !defined(OMR_ENV_DATA64)
	for (i = 0; i < 16; i++) {
		farJumpInfo->__ji_hr[i] = sigContextInfo->__mc_hr_[i];
	}
#endif
	for (i = 0; i < 16; i++) {
		farJumpInfo->__ji_fpr[i] = sigContextInfo->__mc_fpr[i];
	}

	farJumpInfo->__ji_fpc = sigContextInfo->__mc_fpc ;
}


uint32_t
infoForSignal(struct OMRPortLibrary *portLibrary, OMRUnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	switch (index) {

	case OMRPORT_SIG_SIGNAL_TYPE:
	case 0:
		*name = "J9Generic_Signal_Number";
		*value = &info->portLibrarySignalType;
		return OMRPORT_SIG_VALUE_32;

	case OMRPORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE:
	case 1:
		*name = "Signal_Number";
		*value = &info->sigInfo->si_signo;
		return OMRPORT_SIG_VALUE_32;

	case OMRPORT_SIG_SIGNAL_ERROR_VALUE:
	case 2:
		*name = "Error_Value";
		*value = &info->sigInfo->si_errno;
		return OMRPORT_SIG_VALUE_32;

	case OMRPORT_SIG_SIGNAL_CODE:
	case 3:
		*name = "Signal_Code";
		*value = &info->sigInfo->si_code;
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
infoForFPR(struct OMRPortLibrary *portLibrary, OMRUnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	const char *n_fpr[NUM_REGS] = {
		"fpr0",
		"fpr1",
		"fpr2",
		"fpr3",
		"fpr4",
		"fpr5",
		"fpr6",
		"fpr7",
		"fpr8",
		"fpr9",
		"fpr10",
		"fpr11",
		"fpr12",
		"fpr13",
		"fpr14",
		"fpr15"
	};

	*name = "";

	if ((index >= 0) && (index < NUM_REGS)) {
		*name = n_fpr[index];
		*value = &(info->platformSignalInfo.context->__mc_fpr[index]);
		return OMRPORT_SIG_VALUE_64;
	}

	return OMRPORT_SIG_VALUE_UNDEFINED;
}

uint32_t
infoForGPR(struct OMRPortLibrary *portLibrary, OMRUnixSignalInfo *info, int32_t index, const char **name, void **value)
{

	const char *n_gpr[NUM_REGS * 2] = {
		"gpr0",
		"gpr1",
		"gpr2",
		"gpr3",
		"gpr4",
		"gpr5",
		"gpr6",
		"gpr7",
		"gpr8",
		"gpr9",
		"gpr10",
		"gpr11",
		"gpr12",
		"gpr13",
		"gpr14",
		"gpr15"
#if !defined(OMR_ENV_DATA64)
		,
		"hgpr0",
		"hgpr1",
		"hgpr2",
		"hgpr3",
		"hgpr4",
		"hgpr5",
		"hgpr6",
		"hgpr7",
		"hgpr8",
		"hgpr9",
		"hgpr10",
		"hgpr11",
		"hgpr12",
		"hgpr13",
		"hgpr14",
		"hgpr15"
#endif
	};

	*name = "";

	if ((index >= 0) && (index < NUM_REGS)) {
		*name = n_gpr[index];
		*value = &(info->platformSignalInfo.context->__mc_gr[index]);
		return OMRPORT_SIG_VALUE_ADDRESS;
	}
#if !defined(OMR_ENV_DATA64)
	else if ((index >= NUM_REGS) && (index < NUM_REGS * 2)) {
		*name = n_gpr[index];
		*value = &(info->platformSignalInfo.context->__mc_hr_[index - NUM_REGS]);
		return OMRPORT_SIG_VALUE_ADDRESS;
	}
#endif

	return OMRPORT_SIG_VALUE_UNDEFINED;
}

uint32_t
infoForVR(struct OMRPortLibrary *portLibrary, OMRUnixSignalInfo *info, int32_t index, const char **name, void **value)
{
	const char *const n_vr[NUM_VECTOR_REGS] = {
		"vr0",
		"vr1",
		"vr2",
		"vr3",
		"vr4",
		"vr5",
		"vr6",
		"vr7",
		"vr8",
		"vr9",
		"vr10",
		"vr11",
		"vr12",
		"vr13",
		"vr14",
		"vr15",
		"vr16",
		"vr17",
		"vr18",
		"vr19",
		"vr20",
		"vr21",
		"vr22",
		"vr23",
		"vr24",
		"vr25",
		"vr26",
		"vr27",
		"vr28",
		"vr29",
		"vr30",
		"vr31"
	};

	*name = "";

	if ((! info->platformSignalInfo.context->__mc_fl_vr) ||
		(NULL == info->platformSignalInfo.context->__mc_vr_ext)) {
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
	if ((index >= 0) && (index < NUM_VECTOR_REGS)) {
		__jumpinfo_vr_ext_t_ * const vr_ext = (__jumpinfo_vr_ext_t_ *)
											  info->platformSignalInfo.context->__mc_vr_ext;

		*name = n_vr[index];
		*value = &(vr_ext->__ji_ve_savearea[index]);
		return OMRPORT_SIG_VALUE_128;
	}

	return OMRPORT_SIG_VALUE_UNDEFINED;
}

uint32_t
infoForControl(struct OMRPortLibrary *portLibrary, OMRUnixSignalInfo *info, int32_t index, const char **name, void **value)
{
#if defined(OMR_ENV_DATA64)
	struct __cib *conditionInfoBlock = NULL;
	__mch_t *mchRegs = NULL;
#else
	_CEECIB *conditionInfoBlock = NULL;
	_FEEDBACK cibfc;
	omr_31bit_mch *mchRegs = NULL;
#endif

	*name = "";

	switch (index) {

	case OMRPORT_SIG_CONTROL_S390_FPC:
	case 0:
		*name = "fpc";
		*value = &(info->platformSignalInfo.context->__mc_fpc);
		return OMRPORT_SIG_VALUE_32;
	case 1:
		*name = "psw0";
		if (info->platformSignalInfo.context->__mc_psw_flag) {
			*value = &(info->platformSignalInfo.context->__mc_psw[0]);
			return OMRPORT_SIG_VALUE_ADDRESS;
		} else {
			return OMRPORT_SIG_VALUE_UNDEFINED;
		}
	case OMRPORT_SIG_CONTROL_PC:
	case 2:
		*name = "psw1";
		if (info->platformSignalInfo.context->__mc_psw_flag) {
			*value = &(info->platformSignalInfo.context->__mc_psw[sizeof(uintptr_t)]);
			return OMRPORT_SIG_VALUE_ADDRESS;
		} else {
			return OMRPORT_SIG_VALUE_UNDEFINED;
		}
	case OMRPORT_SIG_CONTROL_SP:
	case 3:
		*name = "sp";
		if (0 != info->platformSignalInfo.context->__mc_int_dsa_flag) {
			*value = &(info->platformSignalInfo.context->__mc_int_dsa);
			return OMRPORT_SIG_VALUE_ADDRESS;
		} else {
			return OMRPORT_SIG_VALUE_UNDEFINED;
		}
	case OMRPORT_SIG_CONTROL_S390_BEA:
	case 4:
#if defined(OMR_ENV_DATA64)
		/* 64-bit: request the condition information block to access the machine context for BEA */
		conditionInfoBlock = __le_cib_get();

		if (NULL != conditionInfoBlock) {
			mchRegs = (__mch_t *)conditionInfoBlock->cib_machine;
			info->platformSignalInfo.breakingEventAddr = *(uintptr_t *)(&mchRegs->__mch_bea);
#else
		/* 31-bit: request the condition information block to access the machine context for BEA */
		CEE3CIB(NULL, &conditionInfoBlock, &cibfc);

		/* verify that CEE3CIB was successful */
		if (0 == _FBCHECK(cibfc , CEE000)) {
			mchRegs = (omr_31bit_mch *)conditionInfoBlock->cib_machine;
			info->platformSignalInfo.breakingEventAddr = *(uintptr_t *)(&mchRegs->bea);
#endif
			*name = "bea";
			*value = &(info->platformSignalInfo.breakingEventAddr);
			return OMRPORT_SIG_VALUE_ADDRESS;
		} else {
			return OMRPORT_SIG_VALUE_UNDEFINED;
		}
	/* Provide access to GPR7 when requested specifically, but
	 * hide it from the iterator by only casing it with OMRPORT_SIG_CONTROL_S390_GPR7
	 * It is available to the iterator in the GPR category anyways.
	 */
	case OMRPORT_SIG_CONTROL_S390_GPR7:
		*name = "gpr7";
		*value = &(info->platformSignalInfo.context->__mc_gr[7]);
		return OMRPORT_SIG_VALUE_ADDRESS;

	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

/*
 * NOTE: because we use _gtca() to get the CAA (common anchor area, all the thread info dangles
 * 	off the caa), this routine must run on the thread that triggered the signal corresponding to @ref info
 *
 */
uint32_t
infoForModule(struct OMRPortLibrary *portLibrary, OMRUnixSignalInfo *info, int32_t index, const char **name, void **value)
{

#if defined(J9ZOS39064)

	*name = "";

	return OMRPORT_SIG_VALUE_UNDEFINED;

#else

	/* Output parameters to CEETBCK */
	_INT4 member_id;
	_INT4 program_unit_name_length = MAX_NAME;
	_INT4 call_instruction_address = 0;
	_INT4 entry_name_length = MAX_NAME;
	_INT4 callers_call_instruction_address = 0;
	void *callers_dsaptr;
	_INT4 callers_dsa_format;
	char statement_id[MAX_NAME];
	_INT4 statement_id_length = MAX_NAME;
	void *cibptr;
	_INT4 main_program;
	struct _FEEDBACK fc;

	/* Input parameters to CEETBCK */
	void *caaptr = (void *)_gtca();	/* see note about _gtca() use in function documentation */
	_INT4 dsa_format = -1; /* we don't know if the interrupted routine was OSLINK or XPLINK */
	void *dsaptr;

	/* We don't know if the failing routine was XPLINK or OSLINK so we don't know if we should look
	 * 	at r4 (XPLINK) or r13 (OSLINK) for the dsa (stack pointer).
	 * Fortunately, in both XPLINK/OSLINK cases, the dsa is stored in __mc_int_dsa, but only if __mc_int_dsa_flag is set */
	if (0 == info->platformSignalInfo.context->__mc_int_dsa_flag) {
		*name = "";
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}

	dsaptr = info->platformSignalInfo.context->__mc_int_dsa;

	/* call CEETBCK */
	ceetbck(
		&dsaptr,
		&dsa_format,
		&caaptr,
		&member_id,
		info->platformSignalInfo.program_unit_name,
		&program_unit_name_length,
		&info->platformSignalInfo.program_unit_address,
		&call_instruction_address,
		info->platformSignalInfo.entry_name,
		&entry_name_length,
		&info->platformSignalInfo.entry_address,
		&callers_call_instruction_address,
		&callers_dsaptr,
		&callers_dsa_format,
		statement_id,
		&statement_id_length,
		&cibptr,
		&main_program,
		&fc
	);

	if (fc.tok_sev != 0) {
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}

	switch (index) {
	case OMRPORT_SIG_MODULE_NAME:
	case 0:
		*name = "Program_Unit_Name";
		if (program_unit_name_length > 0) {
			/* Convert program_unit_name from EBCDIC... */
			info->platformSignalInfo.program_unit_name[program_unit_name_length] = 0;
			*value = (void *)e2a_func(info->platformSignalInfo.program_unit_name, program_unit_name_length);
			return OMRPORT_SIG_VALUE_STRING;
		} else {
			/* This code is to force the Caller to continue with the query,
			   even though the Program-unit-name is empty, and thus pick up the
			   (non-empty) entry-name */
			*value = "";
			return OMRPORT_SIG_VALUE_STRING;
		}
		return OMRPORT_SIG_VALUE_UNDEFINED;
	case 1:
		*name = "Program_Unit_Address";
		*value = (void *)&info->platformSignalInfo.program_unit_address;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 2:
		*name = "Entry_Name";
		if (entry_name_length > 0) {
			/* Convert entry_name from EBCDIC... */
			info->platformSignalInfo.entry_name[entry_name_length] = 0;
			*value = (void *)e2a_func(info->platformSignalInfo.entry_name, entry_name_length);
			return OMRPORT_SIG_VALUE_STRING;
		}
		return OMRPORT_SIG_VALUE_UNDEFINED;
	case 3:
		*name = "Entry_Address";
		*value = (void *)&info->platformSignalInfo.entry_address;
		return OMRPORT_SIG_VALUE_ADDRESS;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}

#endif
}

