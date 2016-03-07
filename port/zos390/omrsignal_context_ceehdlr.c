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

#include "omrportpriv.h"
#include <leawi.h>
#include "omr__le_api.h"
#include "omrsignal_context_ceehdlr.h"
#include "omrceeocb.h"

#ifndef J9ZOS39064
#include "omrceetbck.h"
#endif

#define NUM_REGS 16
#define NUM_VECTOR_REGS 32

/*
 * This was derived from fillInJumpInfo in port/zos390/omrsignal_context.c
 */
void
fillInLEJumpInfo(struct OMRPortLibrary *portLibrary, j9_31bit_mch *regs, void *jumpInfo)
{
	int i = 0;
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
		farJumpInfo->__ji_fl_fp4 = 0;
		farJumpInfo->__ji_fl_fp16 = 0;
	} else {
		farJumpInfo->__ji_fl_fp4 = 1;
		farJumpInfo->__ji_fl_fp16 = 1;
	}
	farJumpInfo->__ji_fl_fpc = 1;
	farJumpInfo->__ji_fl_res1a = 0;

#if !defined(OMR_ENV_DATA64)
	if (0 != (regs->mch_flags & J9MCH_FLAGS_HGPRS_VALID)) {
		/* The J9MCH_FLAGS_HGPRS_VALID bit was set */
		farJumpInfo->__ji_fl_hr	= 1;
	} else {
		farJumpInfo->__ji_fl_hr	= 0;
	}

	for (i = 0; i < 16; i++) {
		farJumpInfo->__ji_hr[i] = regs->hgprs[i];
	}

#endif
	farJumpInfo->__ji_fl_res2 = 0;
	farJumpInfo->__ji_fl_exp = 0;
	farJumpInfo->__ji_fl_res2a = 0;

	if (portLibrary->portGlobals->vectorRegsSupportOn) {
		struct __jumpinfo_vr_ext *const jpi = farJumpInfo->__ji_vr_ext;

		memcpy(jpi->__ji_ve_savearea, regs->vr, NUM_VECTOR_REGS * sizeof(U_128));

		/* For the time being, LE requires this field to be set to zero */
		jpi->__ji_ve_version = 0;
	}

	for (i = 0; i < 16; i++) {
		farJumpInfo->__ji_gr[i] = regs->gprs[i];
	}

	for (i = 0; i < 16; i++) {
		/* the fprs are not laid out 0-15 in memory */
		if (i <= 7) {

			if (0 == (i % 2)) {
				/* even index */
				farJumpInfo->__ji_fpr[i] = regs->fprs_0246[i / 2];
			} else {
				/* odd index */
				farJumpInfo->__ji_fpr[i] = regs->fprs_1357[(i - 1) / 2];
			}

		} else if (8 <= i <= 15) {
			farJumpInfo->__ji_fpr[i] = regs->fprs_8_15[i - 8];
		}
	}

	farJumpInfo->__ji_fpc = regs->fpc ;
}



/* infoForFPR_ceehdlr() follows the same convention as infoForFPR() in port/zos390/omrsignal_context.c */
uint32_t
infoForFPR_ceehdlr(struct OMRPortLibrary *portLibrary, J9LEConditionInfo *info, int32_t index, const char **name, void **value)
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
	j9_31bit_mch *j9mch = (j9_31bit_mch *)info->cib->cib_machine;
	*name = "";


	if (index > 15) {
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}

	*name = n_fpr[index];

	/* the fprs are not laid out 0-15 in memory */
	if (index <= 7) {

		if (0 == (index % 2)) {
			/* even index */
			*value = &(j9mch->fprs_0246[index / 2]);
		} else {
			/* odd index */
			*value = &(j9mch->fprs_1357[(index - 1) / 2]);
		}

	} else if (8 <= index <= 15) {
		*value = &(j9mch->fprs_8_15[index - 8]);
	}

	return OMRPORT_SIG_VALUE_64;

}

/* infoForGPR_ceehdlr() follows the same convention as infoForGPR() in port/zos390/omrsignal_context.c */
uint32_t
infoForGPR_ceehdlr(struct OMRPortLibrary *portLibrary, J9LEConditionInfo *info, int32_t index, const char **name, void **value)
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
	j9_31bit_mch *j9mch = (j9_31bit_mch *)info->cib->cib_machine;

	*name = "";

	if ((index >= 0) && (index < NUM_REGS)) {
		*name = n_gpr[index];
		*value = &(j9mch->gprs[index]);
		return OMRPORT_SIG_VALUE_ADDRESS;
	}
#if !defined(OMR_ENV_DATA64)
	else if ((index >= NUM_REGS) && (index < NUM_REGS * 2)) {
		*name = n_gpr[index];
		*value = &(j9mch->hgprs[index - NUM_REGS]);
		return OMRPORT_SIG_VALUE_ADDRESS;
	}
#endif

	return OMRPORT_SIG_VALUE_UNDEFINED;

}

/* infoForVR_ceehdlr() follows the same convention as infoForGPR() in port/zos390/omrsignal_context.c */
uint32_t
infoForVR_ceehdlr(struct OMRPortLibrary *portLibrary, J9LEConditionInfo *info, int32_t index, const char **name, void **value)
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

	j9_31bit_mch *j9mch = (j9_31bit_mch *)info->cib->cib_machine;

	*name = "";

	if (0 == (j9mch->mch_flags & J9MCH_FLAGS_VR_VALID)) {
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
	if ((index >= 0) && (index < NUM_VECTOR_REGS)) {
		*name = n_vr[index];
		*value = &(j9mch->vr[index]);
		return OMRPORT_SIG_VALUE_128;
	}

	return OMRPORT_SIG_VALUE_UNDEFINED;
}

/* infoForControl_ceehdlr() follows the same convention as infoForControl() in port/zos390/omrsignal_context.c
 * Some case statements for an immediate value are preceded by a fall-through case statement on a constant. This is done so that
 * it can be used both as an iterator (see documentation for omrsig_info() in port/common/omrsignal.c and so that values
 * for a specific register/attribute can requested.
 */
uint32_t
infoForControl_ceehdlr(struct OMRPortLibrary *portLibrary, J9LEConditionInfo *info, int32_t index, const char **name, void **value)
{
	j9_31bit_mch *j9mch = (j9_31bit_mch *)info->cib->cib_machine;
	*name = "";

	switch (index) {
	case OMRPORT_SIG_CONTROL_S390_FPC:
	case 0:
		*name = "fpc";
		*value = &(j9mch->fpc);
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 1:
		*name = "psw0";
		*value = &(j9mch->psw0);
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_PC:
	case 2:
		*name = "psw1";
		*value = &(j9mch->psw1);
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_CONTROL_SP:
	case 3:
		*name = "sp";
		if (0 != (j9mch->mch_flags & J9MCH_FLAGS_INT_SF_VALID)) {
			*value = &(j9mch->interrupt_stack_frame);
			return OMRPORT_SIG_VALUE_ADDRESS;
		} else {
			return OMRPORT_SIG_VALUE_UNDEFINED;
		}
	case OMRPORT_SIG_CONTROL_S390_BEA:
	case 4:
		*name = "bea";
		*value = &(j9mch->bea);
		return OMRPORT_SIG_VALUE_ADDRESS;
	/* Provide access to GPR7 when requested specifically, but
	 * hide it from the iterator by only casing it with OMRPORT_SIG_CONTROL_S390_GPR7
	 * It is available to the iterator in the GPR category anyways.
	 */
	case OMRPORT_SIG_CONTROL_S390_GPR7:
		*name = "gpr7";
		*value = &(j9mch->gprs[7]);
		return OMRPORT_SIG_VALUE_ADDRESS;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

/*
 * infoForModule_ceehdlr()  follows the same convention as infoForModule() in port/zos390/omrsignal_context.c
 * Some case statements for an immediate value are preceded by a fall-through case statement on a constant. This is done so that
 * it can be used both as an iterator (see documentation for omrsig_info() in port/common/omrsignal.c and so that values
 * for a specific register/attribute can requested.
 *
 * NOTE: because we use _gtca() to get the CAA (common anchor area, all the thread info dangles
 * 	off the caa), this routine must run on the thread that triggered the condition corresponding to @ref info
 *
 */
uint32_t
infoForModule_ceehdlr(struct OMRPortLibrary *portLibrary, J9LEConditionInfo *info, int32_t index, const char **name, void **value)
{

#if defined(J9ZOS39064)

	*name = "";

	return OMRPORT_SIG_VALUE_UNDEFINED;

#else
	j9_31bit_mch *j9mch = (j9_31bit_mch *)info->cib->cib_machine;

	/* Input paramaters to CEETBCK
	 * We don't know what the stack format was for the routine that triggered the condition,
	 *  but the DSA (stack pointer) is stored in cib_sv1 for both the XPLINK and OSLINK formats */
	void *dsaptr = (void *)info->cib->cib_sv1;
	_INT4 dsa_format = -1; /* we don't know if the interrupted routine was OSLINK/XPLINK */
	void *caaptr = (void *)_gtca(); /* see note about _gtca() use in function documentation */

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

	/* call CEETBCK */
	ceetbck(
		&dsaptr,
		&dsa_format,
		&caaptr,
		&member_id,
		info->program_unit_name,
		&program_unit_name_length,
		&info->program_unit_address,
		&call_instruction_address,
		info->entry_name,
		&entry_name_length,
		&info->entry_address,
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
			info->program_unit_name[program_unit_name_length] = 0;
			*value = (void *)e2a_func(info->program_unit_name, program_unit_name_length);
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
		*value = (void *)&info->program_unit_address;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_MODULE_FUNCTION_NAME:
	case 2:
		*name = "Entry_Name";
		if (entry_name_length > 0) {
			/* Convert entry_name from EBCDIC... */
			info->entry_name[entry_name_length] = 0;
			*value = (void *)e2a_func(info->entry_name, entry_name_length);
			return OMRPORT_SIG_VALUE_STRING;
		}
		return OMRPORT_SIG_VALUE_UNDEFINED;
	case 3:
		*name = "Entry_Address";
		*value = (void *)&info->entry_address;
		return OMRPORT_SIG_VALUE_ADDRESS;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}

#endif
}

/* infoForSignal_ceehdlr()  follows the same convention as infoForSignal() in port/zos390/omrsignal_context.c
 * Some case statements for an immediate value are preceded by a fall-through case statement on a constant. This is done so that
 * it can be used both as an iterator (see documentation for omrsig_info() in port/common/omrsignal.c and so that values
 * for a specific register/attribute can requested.
 */
uint32_t
infoForSignal_ceehdlr(struct OMRPortLibrary *portLibrary, J9LEConditionInfo *info, int32_t index, const char **name, void **value)
{
	j9_31bit_mch *j9mch = (j9_31bit_mch *)info->cib->cib_machine;
	*name = "";

	switch (index) {

	case OMRPORT_SIG_SIGNAL_TYPE:
	case 0:
		*name = "J9Generic_Signal_Number";
		*value = &info->portLibrarySignalType;
		return OMRPORT_SIG_VALUE_32;
	case 1:
	case OMRPORT_SIG_SIGNAL_ZOS_CONDITION_FACILITY_ID:
		*name = "Condition_Facilty_ID";
		*value = info->facilityID;
		return OMRPORT_SIG_VALUE_STRING;
	case 2:
	case OMRPORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE:
		/* fallthrough */
	case OMRPORT_SIG_SIGNAL_ZOS_CONDITION_MESSAGE_NUMBER:
		*name = "Condition_Message_Number";
		*value = &info->messageNumber;
		return OMRPORT_SIG_VALUE_16;
	case 3:
	case OMRPORT_SIG_SIGNAL_ZOS_CONDITION_SEVERITY:
		*name = "Condition_Severity";
		*value = &(info->cib->cib_cond.tok_sev);
		return OMRPORT_SIG_VALUE_16;
	case 4:
	case OMRPORT_SIG_SIGNAL_HANDLER:
		*name = "Handler1";
		*value = &info->handlerAddress;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 5:
		*name = "Handler2";
		*value = &info->handlerAddress2;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_SIGNAL_ZOS_CONDITION_INFORMATION_BLOCK:
		/* Provide access when requested specifically, but
		 * hide it from the iterator by only casing it on a define
		 */
		*name = "Condition_Information_Block";
		*value = &(info->cib);
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_SIGNAL_ZOS_CONDITION_FEEDBACK_TOKEN:
		/* Provide access when requested specifically, but
		 * hide it from the iterator by only casing it on a define
		 */
		*name = "_FEEDBACK";
		*value = &(info->fc);
		return OMRPORT_SIG_VALUE_ADDRESS;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}
