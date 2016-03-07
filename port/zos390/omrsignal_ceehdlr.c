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

/*
 * omrsignal_ceehdlr provides an implementation of signal support that uses LE condition handling
 * it is only to be used on 31-bit z/OS
 **/

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
#define J9SIGNAL_DEBUG
#endif

/* in port/unix/omrsignal.c */
extern uint32_t signalOptionsGlobal;

void j9vm_le_condition_handler(_FEEDBACK *, _INT4 *, _INT4 *, _FEEDBACK *);
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
 *
 * Much of the following j9vm_le_condition_handler parameter documentation is from:
 *
 * 	"V1R11.0 Language Environment Programming Guide", p. 243, http://publibz.boulder.ibm.com/epubs/pdf/ceea21a0.pdf
 *
 *	@param[in]	fc		A 12-byte condition token that identifies the current condition being processed.
 * 							Language Environment uses this parameter to tell your condition handler what condition has occurred.
 *	@param[in]	token 	An J9ZOSLEConditionHandlerRecord as specified when this handler was registered by @ref omrsig_protect_ceehdlr
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
 *
 * The Language environment documentation for all the condition messages are here http://publibz.boulder.ibm.com/epubs/pdf/ceea91a0.pdf,
 * 	but the POSIX signal mappings are not included for hardware raised signals.
 *
 * The messages for hardware raised POSIX signals are in Table 74, pg. 421 here http://publibz.boulder.ibm.com/epubs/pdf/cbcpg1a0.pdf
 * The messages for software raised POSIX signals are in Table 76, pg. 423 here http://publibz.boulder.ibm.com/epubs/pdf/cbcpg1a0.pdf
 *
*/
void
j9vm_le_condition_handler(_FEEDBACK *fc, _INT4 *token, _INT4 *leResult, _FEEDBACK *newfc)
{
	struct J9ZOSLEConditionHandlerRecord *thisRecord = (struct J9ZOSLEConditionHandlerRecord *)*token;
	uint32_t handlerResult;
	_FEEDBACK ceemrcrFc;
	_INT4 ceemrcrType = 0;
	uint32_t portlibSignalNo = OMRPORT_SIG_FLAG_DOES_NOT_MAP_TO_POSIX;
	uint32_t prevPortlibSignalNo = 0;

	/* this needs to an EBCDIC string */
#pragma convlit(suspend)
	const char *ceeEbcdic = "CEE";
#pragma convlit(resume)

#if defined(J9SIGNAL_DEBUG)
	static int count = 0;

	printf("j9vm_le_condition_handler count: %i, %s%i, sev: %i\n", count, e2a_func(fc->tok_facid, 3), fc->tok_msgno, fc->tok_sev);
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
	 * If at this point portlibSignalNo == OMRPORT_SIG_FLAG_DOES_NOT_MAP_TO_POSIX, we have received a condition that cannot be mapped to a harware-raised POSIX signal.
	 * 	- in this case, only handlers that were registered against OMRPORT_SIG_FLAG_SIGALLSYNC should get called,
	 *
	 * If instead the portlibSignalNo has been set, then simply call handlers that had registered explicitly for that
	 * 	signal type
	 */
	if ((0 != (thisRecord->flags & portlibSignalNo))
	 || ((OMRPORT_SIG_FLAG_DOES_NOT_MAP_TO_POSIX == portlibSignalNo)
	  && (0 != (thisRecord->flags & OMRPORT_SIG_FLAG_SIGALLSYNC)))
	) {

		J9LEConditionInfo j9Info;
		_CEECIB *cib_ptr = NULL;
		_FEEDBACK cibfc;

		memset(&j9Info, 0, sizeof(j9Info));
		memset(&cibfc, 0, sizeof(_FEEDBACK));

		/* get the condition information block so that we can access the machine context */
		CEE3CIB(fc, &cib_ptr, &cibfc);

		/* verify that CEE3CIB was successful */
		if (_FBCHECK(cibfc, CEE000) != 0) {
			/* TODO: what do we do here on failure? */
		}

		/* fill in the cookie needed by omrsig_info() */
		j9Info.fc = fc;
		j9Info.cib = cib_ptr;
		j9Info.portLibrarySignalType = portlibSignalNo;
		j9Info.handlerAddress = (void *)thisRecord->handler;
		j9Info.handlerAddress2 = (void *)j9vm_le_condition_handler;
		j9Info.messageNumber = (uint16_t)fc->tok_msgno;
		j9Info.facilityID = e2a_func(fc->tok_facid, 3);	/* e2a_func() uses malloc, free below */

		/* Save the previous signal, set the current, restore the previous
		 * If thisRecord->handler crashes without registering its own protection (using omrsig_protect),
		 *  j9vm_le_condition_handler will be invoked again with thisRecord,
		 *  and the recursive check (at top of function) will reset it to zero.
		 */
		prevPortlibSignalNo = omrsig_get_current_signal_ceehdlr(thisRecord->portLibrary);
		setCurrentSignal(thisRecord->portLibrary, portlibSignalNo);

		thisRecord->recursiveCheck = 1;
		handlerResult = thisRecord->handler(thisRecord->portLibrary, portlibSignalNo, &j9Info, thisRecord->handler_arg);
		thisRecord->recursiveCheck = 0;

		/* control leaves j9vm_le_condition_handler in all cases following this point, so restore the previous signal */
		setCurrentSignal(thisRecord->portLibrary, prevPortlibSignalNo);

		if (NULL != j9Info.facilityID) {
			/* free the memory allocated by e2a_func() */
			free(j9Info.facilityID);
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

	struct J9ZOSLEConditionHandlerRecord thisRecord;
	_FEEDBACK fc;
	_ENTRY leConditionHandler;
	_INT4 token;

#if defined(J9SIGNAL_DEBUG)
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

		/* Register j9vm_le_condition_handler as an LE condition handler */
		token = (_INT4)&thisRecord;
		leConditionHandler.address = (_POINTER)&j9vm_le_condition_handler ;
		leConditionHandler.nesting = NULL;

		CEEHDLR(&leConditionHandler, &token, &fc);

		/* verify that CEEHDLR was successful */
		if (_FBCHECK(fc, CEE000) != 0) {
#if defined(J9SIGNAL_DEBUG)
			printf("CEEHDLR failed with message number %d\n", fc.tok_msgno);
			fflush(stdout);
#endif
			Trc_PRT_zos_omrsig_protect_using_ceehdlr_CEEHDLR_failed(fc.tok_msgno);
			return OMRPORT_SIG_ERROR;
		}

		/* j9vm_le_condition_handler will jump here in the case of OMRPORT_SIG_EXCEPTION_RETURN */
		if (setjmp(thisRecord.returnBuf)) {
			/* j9vm_le_condition_handler jumped back here */
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
}

void
ceehdlr_shutdown(struct OMRPortLibrary *portLibrary)
{
	omrthread_tls_free(tlsKeyCurrentSignal);
}
