/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

/*
 * omrsignal_ceehdlr provides an implementation of signal support that uses LE condition handling
 * it is only to be used on 31-bit z/OS
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "omrportpriv.h"
#include "ut_omrport.h"
#include "omrsignal_context_ceehdlr.h"
#include "omrsignal_ceehdlr.h"
#include "atoe.h"
#include "portnls.h"

#if 0
#define OMRSIGNAL_DEBUG
#endif

/* in port/unix/omrsignal.c */
extern uint32_t signalOptionsGlobal;

void omrsig_le_condition_handler(_FEEDBACK *, _INT4 *, _INT4 *, _FEEDBACK *);
static uint32_t mapZOS390ExceptionToPortlibType(uint32_t exceptionCode);

static uint32_t
mapZOS390ExceptionToPortlibType(uint32_t exceptionCode)
{
	return 0;
}

/* key to get the current synchronous signal */
static omrthread_tls_key_t tlsKeyCurrentSignal;
static void setCurrentSignal(struct OMRPortLibrary *portLibrary, uint32_t portLibSigType);

/*
 * Much of the following omrsig_le_condition_handler parameter documentation is from:
 *
 * 	"V1R11.0 Language Environment Programming Guide", p. 243, http://publibz.boulder.ibm.com/epubs/pdf/ceea21a0.pdf
 *
 *	@param[in]	fc		A 12-byte condition token that identifies the current condition being processed.
 * 							Language Environment uses this parameter to tell your condition handler what condition has occurred.
 *	@param[in]	token 	An OMRZOSLEConditionHandlerRecord as specified when this handler was registered by @ref omrsig_protect_ceehdlr
 *	@param[in]	leResult	A 4-byte integer that contains instructions about responses the user-written condition
 * 							handler wants Language Environment to make when processing the condition.
 * 							The result_code is passed by reference.
 * 							Valid responses are shown in Table 38 of the Language Environment Programming Guide
 * 							For example, use 20 to percolate the condition.
 *	@param[in]	newfc	new_condition (output) A 12-byte condition token that represents either
 *						 the promoted condition for a promote response (result_code values of 30, 31, and 32)
 *						 or the requested fix-up actions for a fix-up and resume response (result_code value of 60).
 *						 When a result_code of 60, denoting fix-up and resume, is set by the condition handler,
 *						 new_condition must be set to a condition token that indicates what fix-up action is requested.
 *						 Many conditions, including mathematical routines, use the condition tokens in
 *						 Table 39 to resume with corrective action (either resume with new input value
 *						 or resume with new output value).
 *						 For some conditions, there may be other condition tokens that can be provided by the condition
 *						 handler in new_condition to request specific fix-up actions.
 *
 * The Language environment documentation for all the condition messages are here http://publibz.boulder.ibm.com/epubs/pdf/ceea91a0.pdf,
 * but the POSIX signal mappings are not included for hardware raised signals.
 *
 * The messages for hardware raised POSIX signals are in Table 74, pg. 421 here http://publibz.boulder.ibm.com/epubs/pdf/cbcpg1a0.pdf
 * The messages for software raised POSIX signals are in Table 76, pg. 423 here http://publibz.boulder.ibm.com/epubs/pdf/cbcpg1a0.pdf
 */
void
omrsig_le_condition_handler(_FEEDBACK *fc, _INT4 *token, _INT4 *leResult, _FEEDBACK *newfc)
{
	struct OMRZOSLEConditionHandlerRecord *thisRecord = (struct OMRZOSLEConditionHandlerRecord *)*token;
	uint32_t handlerResult;
	_FEEDBACK ceemrcrFc;
	_INT4 ceemrcrType = 0;
	uint32_t portlibSignalNo = 0;
	uint32_t prevPortlibSignalNo = 0;

	/* this needs to an EBCDIC string */
#pragma convlit(suspend)
	const char *ceeEbcdic = "CEE";
#pragma convlit(resume)

#if defined(OMRSIGNAL_DEBUG)
	static int count = 0;

	printf("omrsig_le_condition_handler count: %i, %s%i, sev: %i\n", count, e2a_func(fc->tok_facid, 3), fc->tok_msgno, fc->tok_sev);
	fflush(NULL);
	count++;
#endif

	if (1 == thisRecord->recursiveCheck) {
		/* This handler has been invoked recursively, percolate, but first reset the current signal.
		 * Any handler that was already handling a condition will also percolate, until we reach
		 *  a handler that was not already handling a condition,
		 *  at which point there is no notion of a previous signal.
		 */
		setCurrentSignal(thisRecord->portLibrary, 0);
		*leResult = 20;
		return;
	}

	if (fc->tok_sev <= 1) {
		/* this is an information only condition, percolate */
		*leResult = 20;
		return;
	}

	if (0 == strncmp(ceeEbcdic, fc->tok_facid, 3)) {
		if ((3207 <= fc->tok_msgno) && (fc->tok_msgno <= 3234)) {
			/* hardware-raised SIGFPE */
			portlibSignalNo = OMRPORT_SIG_FLAG_SIGFPE;
		} else if ((3201 <= fc->tok_msgno) && (fc->tok_msgno <= 3203)) {
			/* hardware-raised SIGILL */
			portlibSignalNo = OMRPORT_SIG_FLAG_SIGILL;
		} else if ((3204 <= fc->tok_msgno) && (fc->tok_msgno <= 3206)) {
			/* hardware-raised SIGSEGV */
			portlibSignalNo = OMRPORT_SIG_FLAG_SIGSEGV;
		} else {
			if ((5201 <= fc->tok_msgno) && (fc->tok_msgno <= 5238)) {
				OMRPortLibrary *portLibrary = thisRecord->portLibrary;
				char *asciiFacilityID = e2a_func(fc->tok_facid, 3);
				portLibrary->nls_printf(portLibrary, J9NLS_ERROR, J9NLS_PORT_ZOS_CONDITION_FOR_SOFTWARE_RAISED_SIGNAL_RECEIVED, (NULL == asciiFacilityID) ? "NULL" : asciiFacilityID, fc->tok_msgno);
				free(asciiFacilityID);
			}
		}
	}

	/*
	 * Call the handlers that were registered with the port library.
	 *
	 * If portlibSignalNo is 0 at this point, we have received a condition that cannot be mapped to a hardware-raised POSIX signal.
	 * In this case, only handlers that were registered against OMRPORT_SIG_FLAG_SIGALLSYNC should get called,
	 *
	 * If portlibSignalNo is set, then call handlers registered explicitly for the specified signal type.
	 */
	if (OMR_ARE_ANY_BITS_SET(thisRecord->flags, portlibSignalNo)
	 || ((0 == portlibSignalNo) && OMR_ARE_ANY_BITS_SET(thisRecord->flags, OMRPORT_SIG_FLAG_SIGALLSYNC))
	) {
		OMRLEConditionInfo signalInfo;
		_CEECIB *cib_ptr = NULL;
		_FEEDBACK cibfc;

		memset(&signalInfo, 0, sizeof(signalInfo));
		memset(&cibfc, 0, sizeof(_FEEDBACK));

		/* get the condition information block so that we can access the machine context */
		CEE3CIB(fc, &cib_ptr, &cibfc);

		/* verify that CEE3CIB was successful */
		if (_FBCHECK(cibfc, CEE000) != 0) {
			/* TODO: what do we do here on failure? */
		}

		/* fill in the cookie needed by omrsig_info() */
		signalInfo.fc = fc;
		signalInfo.cib = cib_ptr;
		signalInfo.portLibrarySignalType = portlibSignalNo;
		signalInfo.handlerAddress = (void *)thisRecord->handler;
		signalInfo.handlerAddress2 = (void *)omrsig_le_condition_handler;
		signalInfo.messageNumber = (uint16_t)fc->tok_msgno;
		signalInfo.facilityID = e2a_func(fc->tok_facid, 3);	/* e2a_func() uses malloc, free below */

		/* Save the previous signal, set the current, restore the previous
		 * If thisRecord->handler crashes without registering its own protection (using omrsig_protect),
		 *  omrsig_le_condition_handler will be invoked again with thisRecord,
		 *  and the recursive check (at top of function) will reset it to zero.
		 */
		prevPortlibSignalNo = omrsig_get_current_signal_ceehdlr(thisRecord->portLibrary);
		setCurrentSignal(thisRecord->portLibrary, portlibSignalNo);

		thisRecord->recursiveCheck = 1;
		handlerResult = thisRecord->handler(thisRecord->portLibrary, portlibSignalNo, &signalInfo, thisRecord->handler_arg);
		thisRecord->recursiveCheck = 0;

		/* control leaves omrsig_le_condition_handler in all cases following this point, so restore the previous signal */
		setCurrentSignal(thisRecord->portLibrary, prevPortlibSignalNo);

		if (NULL != signalInfo.facilityID) {
			/* free the memory allocated by e2a_func() */
			free(signalInfo.facilityID);
		}

		switch (handlerResult) {
		case OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH:
			/* thisRecord->handler was not adequately able to address the signal, try the previously registered handler
			 * To do this, percolate the signal such as to invoke the previously registered LE condition handler */
			*leResult = 20; /* percolate */
			return;
		case OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION: {
			struct __jumpinfo_vr_ext farJumpVRInfo;

			thisRecord->farJumpInfo.__ji_vr_ext = &farJumpVRInfo;

			fillInLEJumpInfo(thisRecord->portLibrary, cib_ptr->cib_machine, &(thisRecord->farJumpInfo));
			__far_jump(&(thisRecord->farJumpInfo));
			/* unreachable */
			break;
		}
		case OMRPORT_SIG_EXCEPTION_RETURN:
			/* jump back to where this was registered in @ref omrsig_protect_ceehdlr, so that we can return back to it's caller  */
			longjmp(thisRecord->returnBuf, 0);
			/* unreachable */
			break;
		}
	} else {
		/* the handler doesn't want to handle this signal, percolate */
		*leResult = 20;
		return;
	}
}

int32_t
omrsig_protect_ceehdlr(struct OMRPortLibrary *portLibrary,  omrsig_protected_fn fn, void *fn_arg, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, uintptr_t *result)
{

	struct OMRZOSLEConditionHandlerRecord thisRecord;
	_FEEDBACK fc;
	_ENTRY leConditionHandler;
	_INT4 token;

#if defined(OMRSIGNAL_DEBUG)
	printf(" \nomrsig_protect_ceehdlr\n");
	fflush(NULL);
#endif

	if (0 == (signalOptionsGlobal & OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS)) {

		memset(&fc, 0, sizeof(_FEEDBACK));
		memset(&leConditionHandler, 0, sizeof(_ENTRY));
		memset(&thisRecord, 0, sizeof(thisRecord));
		token = 0;

		thisRecord.portLibrary = portLibrary;
		thisRecord.handler = handler;
		thisRecord.handler_arg = handler_arg;
		thisRecord.flags = flags;

		/* Register omrsig_le_condition_handler as an LE condition handler */
		token = (_INT4)&thisRecord;
		leConditionHandler.address = (_POINTER)&omrsig_le_condition_handler ;
		leConditionHandler.nesting = NULL;

		CEEHDLR(&leConditionHandler, &token, &fc);

		/* verify that CEEHDLR was successful */
		if (_FBCHECK(fc, CEE000) != 0) {
#if defined(OMRSIGNAL_DEBUG)
			printf("CEEHDLR failed with message number %d\n", fc.tok_msgno);
			fflush(stdout);
#endif
			Trc_PRT_zos_omrsig_protect_using_ceehdlr_CEEHDLR_failed(fc.tok_msgno);
			return OMRPORT_SIG_ERROR;
		}

		/* omrsig_le_condition_handler will jump here in the case of OMRPORT_SIG_EXCEPTION_RETURN */
		if (setjmp(thisRecord.returnBuf)) {
			/* omrsig_le_condition_handler jumped back here */
			*result = 0;
			return OMRPORT_SIG_EXCEPTION_OCCURRED;
		}
	}

	*result = fn(portLibrary, fn_arg);

	return 0;
}

uint32_t
omrsig_info_ceehdlr(struct OMRPortLibrary *portLibrary, void *info, uint32_t category, int32_t index, const char **name, void **value)
{

	*name = "";

	switch (category) {
	case OMRPORT_SIG_GPR:
		return infoForGPR_ceehdlr(portLibrary, info, index, name, value);
	case OMRPORT_SIG_CONTROL:
		return infoForControl_ceehdlr(portLibrary, info, index, name, value);
	case OMRPORT_SIG_FPR:
		return infoForFPR_ceehdlr(portLibrary, info, index, name, value);
	case OMRPORT_SIG_MODULE:
		return infoForModule_ceehdlr(portLibrary, info, index, name, value);
	case OMRPORT_SIG_SIGNAL:
		return infoForSignal_ceehdlr(portLibrary, info, index, name, value);
	case OMRPORT_SIG_VR:
		if (portLibrary->portGlobals->vectorRegsSupportOn) {
			return infoForVR_ceehdlr(portLibrary, info, index, name, value);
		}
	case OMRPORT_SIG_OTHER:
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

intptr_t
omrsig_get_current_signal_ceehdlr(struct OMRPortLibrary *portLibrary)
{
	return (intptr_t)omrthread_tls_get(omrthread_self(), tlsKeyCurrentSignal);
}

static void
setCurrentSignal(struct OMRPortLibrary *portLibrary, uint32_t portLibSigType)
{
	omrthread_tls_set(omrthread_self(), tlsKeyCurrentSignal, (intptr_t *)portLibSigType);
}

int32_t
ceehdlr_startup(struct OMRPortLibrary *portLibrary)
{
	/* use this to record the last signal that occured such that we can call omrsig_handler in omrexit_shutdown_and_exit */
	if (omrthread_tls_alloc(&tlsKeyCurrentSignal)) {
		return -1;
	}
	return 0;
}

void
ceehdlr_shutdown(struct OMRPortLibrary *portLibrary)
{
	omrthread_tls_free(tlsKeyCurrentSignal);
}
