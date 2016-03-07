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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/


/*
 * $RCSfile: omrsignalTest.c,v $
 * $Revision: 1.66 $
 * $Date: 2013-03-21 14:42:10 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library signal handling.
 *
 * Exercise the API for port library signal handling.  These functions
 * can be found in the file @ref omrsignal.c
 *
 */

#if !defined(WIN32)
#include <signal.h>
#endif

#if defined(J9ZOS390)
#include "atoe.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "fltconst.h"
#include "omrthread.h"


#include "testHelpers.hpp"
#include "testProcessHelpers.hpp"
#include "omrport.h"

#if !defined(WIN32) && defined(OMR_PORT_ASYNC_HANDLER)
#define J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS
#endif

#define SIG_TEST_SIZE_EXENAME 1024

#if defined(J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS)

extern PortTestEnvironment *portTestEnv;

struct PortSigMap {
	uint32_t portLibSignalNo;
	const char *portLibSignalString ;
	int unixSignalNo;
	const char *unixSignalString;
} PortSigMap;
struct PortSigMap testSignalMap[] = {
	{OMRPORT_SIG_FLAG_SIGQUIT, "OMRPORT_SIG_FLAG_SIGQUIT", SIGQUIT, "SIGQUIT"}
	, {OMRPORT_SIG_FLAG_SIGABRT, "OMRPORT_SIG_FLAG_SIGABRT", SIGABRT, "SIGABRT"}
	, {OMRPORT_SIG_FLAG_SIGTERM, "OMRPORT_SIG_FLAG_SIGTERM", SIGTERM, "SIGTERM"}
#if defined(AIXPPC)
	, {OMRPORT_SIG_FLAG_SIGRECONFIG, "OMRPORT_SIG_FLAG_SIGRECONFIG", SIGRECONFIG, "SIGRECONFIG"}
#endif
	/* {OMRPORT_SIG_FLAG_SIGINT, "OMRPORT_SIG_FLAG_SIGINT", SIGINT, "SIGINT"} */
};

typedef struct AsyncHandlerInfo {
	uint32_t expectedType;
	const char *testName;
	omrthread_monitor_t *monitor;
	uint32_t controlFlag;
} AsyncHandlerInfo;

static uintptr_t asyncTestHandler(struct OMRPortLibrary *portLibrary, uint32_t gpType, void *gpInfo, void *userData);
static void injectUnixSignal(struct OMRPortLibrary *portLibrary, int pid, int unixSignal);

#endif /* J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS */

typedef struct SigProtectHandlerInfo {
	uint32_t expectedType;
	uintptr_t returnValue;
	const char *testName;
} SigProtectHandlerInfo;

static U_32 portTestOptionsGlobal;

#if defined(J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS)

/*
 * Sets  the controlFlag to 1 such that we can test that it was actually invoked
 *
 * Synchronizes using AsyncHandlerInfo->monitor
 *
 */
static uintptr_t
asyncTestHandler(struct OMRPortLibrary *portLibrary, uint32_t gpType, void *handlerInfo, void *userData)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	AsyncHandlerInfo *info = (AsyncHandlerInfo *) userData;
	const char *testName = info->testName;
	omrthread_monitor_t monitor = *(info->monitor);

	omrthread_monitor_enter(monitor);

	outputComment(OMRPORTLIB, "\t\tasyncTestHandler invoked (type = 0x%x)\n", gpType);
	if (info->expectedType != gpType) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "asyncTestHandler -- incorrect type. Expecting 0x%x, got 0x%x\n", info->expectedType, gpType);
	}

	info->controlFlag = 1;
	omrthread_monitor_notify(monitor);
	omrthread_monitor_exit(monitor);
	return 0;

}

static void
injectUnixSignal(struct OMRPortLibrary *portLibrary, int pid, int unixSignal)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	/* this is run by the child process. To see the tty_printf output on the console, set OMRPORT_PROCESS_INHERIT_STDOUT | OMRPORT_PROCESS_INHERIT_STDIN
	 * in the options passed into j9process_create() in launchChildProcess (in testProcessHelpers.c).
	 */
	omrtty_printf("\t\tCHILD:	 calling kill: %i %i\n", pid, unixSignal);
	kill(pid, unixSignal);
	return;
}

#endif /* J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS */

/**
 * Verify port library signal handling.
 *
 * @param[in] portLibrary 	the port library under test
 * @param[in] exeName 		the executable that invoked the test
 * @param[in  arg			either NULL or of the form omrsig_injectSignal_<pid>_<signal>
 *
 * @return 0 on success, -1 on failure
 */
int32_t
omrsig_runTests(struct OMRPortLibrary *portLibrary, char *exeName, char *argument)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	portTestOptionsGlobal = omrsig_get_options();

#if defined(J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS)
	if (argument != NULL) {
		if (1 == startsWith(argument, "omrsig_injectSignal")) {
			char *scratch;
			int pid, unixSignal;
			char *strTokSavePtr;

			/* consume "omrsig" */
			(void)strtok_r(argument, "_", &strTokSavePtr);

			/* consume "injectSignal" */
			(void)strtok_r(NULL, "_", &strTokSavePtr);

			/* consume PID */
			scratch = strtok_r(NULL, "_", &strTokSavePtr);
			pid = atoi(scratch);

			/* consume signal number */
			scratch = strtok_r(NULL, "_", &strTokSavePtr);
			unixSignal = atoi(scratch);

			injectUnixSignal(OMRPORTLIB, pid, unixSignal);
			return TEST_PASS /* doesn't matter what we return here */;
		}
	}
#endif
	return 0;
}

/*
 * Test that a named register has the correct type, and exists correctly.
 * If platform is not NULL and is not the current platform, the register MUST be undefined.
 * Otherwise the register must be expectedKind, or it may be undefined if optional is TRUE.
 */
static void
validatePlatformRegister(struct OMRPortLibrary *portLibrary, void *gpInfo, uint32_t category, int32_t index, uint32_t expectedKind, uintptr_t optional, const char *platform, const char *testName)
{
	uint32_t infoKind;
	void *value;
	const char *name;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	infoKind = omrsig_info(gpInfo, category, index, &name, &value);
	if ((platform == NULL) || (strcmp(platform, omrsysinfo_get_CPU_architecture()) == 0)) {
		if (infoKind == OMRPORT_SIG_VALUE_UNDEFINED) {
			if (optional == FALSE) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Required platform register %d is undefined\n", index);
			}
		} else if (infoKind != expectedKind) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Platform register %d type is not %u. It is %u\n", index, expectedKind, infoKind);
		}
	} else {
		if (infoKind != OMRPORT_SIG_VALUE_UNDEFINED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "OMRPORT_SIG_GPR_X86_EDI type is not OMRPORT_SIG_VALUE_UNDEFINED. It is %u\n", infoKind);
		}
	}
}

void
validateGPInfo(struct OMRPortLibrary *portLibrary, uint32_t gpType, omrsig_handler_fn handler, void *gpInfo, const char *testName)
{
	uint32_t category;
	void *value;
	const char *name;
	uint32_t index;
	uint32_t infoKind;
	const char *arch;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	for (category = 0; category < OMRPORT_SIG_NUM_CATEGORIES; category++) {
		uint32_t infoCount;

		infoCount = omrsig_info_count(gpInfo, category);

		for (index = 0; index < infoCount; index++) {
			infoKind = omrsig_info(gpInfo, category, index, &name, &value);

			switch (infoKind) {
			case OMRPORT_SIG_VALUE_16:
				outputComment(OMRPORTLIB, "info %u:%u: %s=%04X\n", category, index, name, *(U_16 *)value);
				break;
			case OMRPORT_SIG_VALUE_32:
				outputComment(OMRPORTLIB, "info %u:%u: %s=%08.8x\n", category, index, name, *(uint32_t *)value);
				break;
			case OMRPORT_SIG_VALUE_64:
				outputComment(OMRPORTLIB, "info %u:%u: %s=%016.16llx\n", category, index, name, *(uint64_t *)value);
				break;
			case OMRPORT_SIG_VALUE_STRING:
				outputComment(OMRPORTLIB, "info %u:%u: %s=%s\n", category, index, name, (const char *)value);
				break;
			case OMRPORT_SIG_VALUE_ADDRESS:
				outputComment(OMRPORTLIB, "info %u:%u: %s=%p\n", category, index, name, *(void **)value);
				break;
			case OMRPORT_SIG_VALUE_FLOAT_64:
				/* make sure when casting to a float that we get least significant 32-bits. */
				outputComment(OMRPORTLIB,
							  "info %u:%u: %s=%016.16llx (f: %f, d: %e)\n",
							  category, index, name,
							  *(uint64_t *)value, (float)LOW_U32_FROM_DBL_PTR(value), *(double *)value);
				break;
				case OMRPORT_SIG_VALUE_128:
					{
						const U_128 *const v = (U_128 *)value;
						const uint64_t h = v->high64;
						const uint64_t l = v->low64;

						outputComment(OMRPORTLIB, "info %u:%u: %s=%016.16llx%016.16llx\n", category, index, name, h, l);
					}
				break;
			case OMRPORT_SIG_VALUE_UNDEFINED:
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Undefined value in info (%u:%u, name=%s)\n", category, index, name);
				break;
			default:
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Unrecognized type in info (%u:%u, name=%s, type=%u)\n", category, index, name, infoKind);
				break;
			}
		}

		infoKind = omrsig_info(gpInfo, category, index, &name, &value);
		if (infoKind != OMRPORT_SIG_VALUE_UNDEFINED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected data after end of info (%u:%u, name=%s, type=%u)\n", category, index, name, infoKind);
		}
	}

	/* test the known names */
	infoKind = omrsig_info(gpInfo, OMRPORT_SIG_SIGNAL, OMRPORT_SIG_SIGNAL_TYPE, &name, &value);
	if (infoKind != OMRPORT_SIG_VALUE_32) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "OMRPORT_SIG_SIGNAL_TYPE type is not OMRPORT_SIG_VALUE_32. It is %u\n", infoKind);
	} else if (*(uint32_t *)value != gpType) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "OMRPORT_SIG_SIGNAL_TYPE is incorrect. Expecting %u, got %u\n", gpType, *(uint32_t *)value);
	}

	infoKind = omrsig_info(gpInfo, OMRPORT_SIG_SIGNAL, OMRPORT_SIG_SIGNAL_HANDLER, &name, &value);
	if (infoKind != OMRPORT_SIG_VALUE_ADDRESS) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "OMRPORT_SIG_SIGNAL_HANDLER type is not OMRPORT_SIG_VALUE_ADDRESS. It is %u\n", infoKind);
	} else if (*(omrsig_handler_fn *)value != handler) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "OMRPORT_SIG_SIGNAL_HANDLER is incorrect. Expecting %p, got %p\n", handler, *(omrsig_handler_fn *)value);
	}

	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_MODULE, OMRPORT_SIG_MODULE_NAME, OMRPORT_SIG_VALUE_STRING, TRUE, NULL, testName);

	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_SIGNAL, OMRPORT_SIG_SIGNAL_ERROR_VALUE, OMRPORT_SIG_VALUE_32, TRUE, NULL, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_SIGNAL, OMRPORT_SIG_SIGNAL_ADDRESS, OMRPORT_SIG_VALUE_ADDRESS, TRUE, NULL, testName);

	portTestOptionsGlobal = omrsig_get_options();
	if (0 != (OMRPORT_SIG_OPTIONS_ZOS_USE_CEEHDLR & portTestOptionsGlobal)) {
		/* The LE condition handler message number is a U_16 */
		validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_SIGNAL, OMRPORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE, OMRPORT_SIG_VALUE_16, TRUE, NULL, testName);
	} else {
		validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_SIGNAL, OMRPORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE, OMRPORT_SIG_VALUE_32, TRUE, NULL, testName);
	}

	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_SIGNAL, OMRPORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS, OMRPORT_SIG_VALUE_ADDRESS, TRUE, NULL, testName);

	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_PC, OMRPORT_SIG_VALUE_ADDRESS, FALSE, NULL, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_SP, OMRPORT_SIG_VALUE_ADDRESS, TRUE, NULL, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_BP, OMRPORT_SIG_VALUE_ADDRESS, TRUE, NULL, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_S390_BEA, OMRPORT_SIG_VALUE_ADDRESS, TRUE, NULL, testName);


	/*
	 * x86 registers
	 */
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_X86_EDI, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_X86, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_X86_ESI, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_X86, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_X86_EAX, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_X86, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_X86_EBX, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_X86, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_X86_ECX, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_X86, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_X86_EDX, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_X86, testName);

	/*
	 * AMD64 registers
	 */
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_RDI, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_RSI, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_RAX, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_RBX, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_RCX, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_RDX, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_R8, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_R9, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_R10, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_R11, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_R12, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_R13, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_R14, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_GPR, OMRPORT_SIG_GPR_AMD64_R15, OMRPORT_SIG_VALUE_ADDRESS, FALSE, OMRPORT_ARCH_HAMMER, testName);

	/*
	 * PowerPC registers
	 */

	/* these registers are valid on both PPC32 and PPC64, so use whichever we're running on if we are on a PPC */
	arch = OMRPORT_ARCH_PPC;
	if (0 == strcmp(OMRPORT_ARCH_PPC64, omrsysinfo_get_CPU_architecture())) {
		arch = OMRPORT_ARCH_PPC64;
	} else if (0 == strcmp(OMRPORT_ARCH_PPC64LE, omrsysinfo_get_CPU_architecture())) {
		arch = OMRPORT_ARCH_PPC64LE;
	}

	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_POWERPC_LR, OMRPORT_SIG_VALUE_ADDRESS, FALSE, arch, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_POWERPC_MSR, OMRPORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_POWERPC_CTR, OMRPORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_POWERPC_CR, OMRPORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_POWERPC_FPSCR, OMRPORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_POWERPC_XER, OMRPORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_POWERPC_DAR, OMRPORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_POWERPC_DSIR, OMRPORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);

	/* this one is PPC32 only */
	validatePlatformRegister(OMRPORTLIB, gpInfo, OMRPORT_SIG_CONTROL, OMRPORT_SIG_CONTROL_POWERPC_MQ, OMRPORT_SIG_VALUE_ADDRESS, TRUE, OMRPORT_ARCH_PPC, testName);
}

static uintptr_t
sampleFunction(OMRPortLibrary *portLibrary, void *arg)
{
	return 8096;
}


static uintptr_t
simpleHandlerFunction(struct OMRPortLibrary *portLibrary, uint32_t gpType, void *gpInfo, void *handler_arg)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	SigProtectHandlerInfo *info = (SigProtectHandlerInfo *)handler_arg;
	const char *testName = info->testName;

	outputComment(OMRPORTLIB, "simpleHandlerFunction invoked (type = 0x%x)\n", gpType);

	if (info->expectedType != gpType) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "simpleHandlerFunction -- incorrect type. Expecting 0x%x, got 0x%x\n", info->expectedType, gpType);
	}
	validateGPInfo(OMRPORTLIB, gpType, simpleHandlerFunction, gpInfo, testName);

	return info->returnValue;
}

static uintptr_t
currentSigNumHandlerFunction(struct OMRPortLibrary *portLibrary, uint32_t gpType, void *gpInfo, void *handler_arg)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	SigProtectHandlerInfo *info = (SigProtectHandlerInfo *)handler_arg;
	intptr_t currentSigNum = omrsig_get_current_signal();
	const char *testName = info->testName;

	if (currentSigNum < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "currentSigNumHandlerFunction -- omrsig_get_current_signal call failed\n");
		return OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH;
	}
	if (gpType != (uint32_t) currentSigNum) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "currentSigNumHandlerFunction -- unexpected value for current signal. Expecting 0x%x, got 0x%x\n", gpType, (uint32_t)currentSigNum);
		return OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH;
	}
	return OMRPORT_SIG_EXCEPTION_RETURN;
}

static uintptr_t
crashingHandlerFunction(struct OMRPortLibrary *portLibrary, uint32_t gpType, void *gpInfo, void *handler_arg)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char *testName = (const char *)handler_arg;
	static int recursive = 0;

	outputComment(OMRPORTLIB, "crashingHandlerFunction invoked (type = 0x%x)\n", gpType);

	if (recursive++) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "handler invoked recursively\n");
		return OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH;
	}

	raiseSEGV(OMRPORTLIB, handler_arg);

	outputErrorMessage(PORTTEST_ERROR_ARGS, "unreachable\n");

	return OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH;
}

static uintptr_t
nestedTestCatchSEGV(OMRPortLibrary *portLibrary, void *arg)
{
	uint32_t flags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_SIGSEGV;
	const char *testName = (const char *)arg;
	int i;
	uint32_t protectResult;
	uintptr_t result;
	SigProtectHandlerInfo handlerInfo;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {
		for (i = 0; i < 3; i++) {
			handlerInfo.expectedType = OMRPORT_SIG_FLAG_SIGSEGV;
			handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_RETURN;
			handlerInfo.testName = testName;
			protectResult = omrsig_protect(
								raiseSEGV, (void *)testName,
								simpleHandlerFunction, &handlerInfo,
								flags,
								&result);

			if (protectResult != OMRPORT_SIG_EXCEPTION_OCCURRED) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected OMRPORT_SIG_EXCEPTION_OCCURRED return value\n");
			}

			if (result != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
			}
		}
	}

	return 8096;
}

static uintptr_t
nestedTestCatchNothing(OMRPortLibrary *portLibrary, void *arg)
{
	uint32_t flags = 0;
	const char *testName = (const char *)arg;
	uintptr_t result;
	SigProtectHandlerInfo handlerInfo;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {
		handlerInfo.expectedType = 0;
		handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		(void)omrsig_protect(
			raiseSEGV, (void *)testName,
			simpleHandlerFunction, &handlerInfo,
			flags,
			&result);

		outputErrorMessage(PORTTEST_ERROR_ARGS, "this should be unreachable\n");
	}

	return 8096;
}

static uintptr_t
nestedTestCatchAndIgnore(OMRPortLibrary *portLibrary, void *arg)
{
	uint32_t flags = OMRPORT_SIG_FLAG_SIGSEGV;
	const char *testName = (const char *)arg;
	uintptr_t result;
	SigProtectHandlerInfo handlerInfo;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {
		handlerInfo.expectedType = OMRPORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		handlerInfo.testName = testName;
		(void)omrsig_protect(
			raiseSEGV, (void *)testName,
			simpleHandlerFunction, &handlerInfo,
			flags,
			&result);

		outputErrorMessage(PORTTEST_ERROR_ARGS, "this should be unreachable\n");
	}

	return 8096;
}

static uintptr_t
nestedTestCatchAndContinueSearch(OMRPortLibrary *portLibrary, void *arg)
{
	uint32_t flags = OMRPORT_SIG_FLAG_SIGSEGV | OMRPORT_SIG_FLAG_MAY_RETURN;
	const char *testName = (const char *)arg;
	uint32_t protectResult;
	uintptr_t result;
	SigProtectHandlerInfo handlerInfo;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {
		handlerInfo.expectedType = OMRPORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		handlerInfo.testName = testName;
		protectResult = omrsig_protect(
							raiseSEGV, (void *)testName,
							simpleHandlerFunction, &handlerInfo,
							flags,
							&result);
		if (protectResult != OMRPORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected OMRPORT_SIG_EXCEPTION_OCCURRED return value\n");
		}
	}

	return 0;
}

static uintptr_t
nestedTestCatchAndCrash(OMRPortLibrary *portLibrary, void *arg)
{
	uint32_t flags = OMRPORT_SIG_FLAG_SIGSEGV;
	const char *testName = (const char *)arg;
	uintptr_t result;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {
		(void)omrsig_protect(
			raiseSEGV, (void *)testName,
			crashingHandlerFunction, (void *)testName,
			flags,
			&result);

		outputErrorMessage(PORTTEST_ERROR_ARGS, "this should be unreachable\n");
	}

	return 8096;
}

/*
 * Test that we can protect against the flags:
 * 	0,
 * 	0 | OMRPORT_SIG_FLAG_MAY_RETURN,
 * 	0 | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
 * 	0 | OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION
 *
*/
TEST(PortSigTest, sig_test0)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrsig_test0";

	reportTestEntry(OMRPORTLIB, testName);

	if (!omrsig_can_protect(0)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->can_protect() must always support 0\n");
	}

	if (!omrsig_can_protect(OMRPORT_SIG_FLAG_MAY_RETURN)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->can_protect() must always support 0 | OMRPORT_SIG_FLAG_MAY_RETURN\n");
	}



	if (!omrsig_can_protect(OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->can_protect() must always support 0 | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION\n");
# if defined(J9ZOS390)
		/* z/OS does not always support this, however, pltest should never be run under a configuration that does not support it.
		 * See implementation of omrsig_can_proctect() and omrsig_startup()
		 */
		omrtty_printf("OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION is not supported if the default settings for TRAP(ON,SPIE) have been changed. The should both be set to 1\n");
#endif
	}


	if (!omrsig_can_protect(OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->can_protect() must always support 0 | OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION\n");
# if defined(J9ZOS390)
		omrtty_printf("OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION is not supported if the default settings for TRAP(ON,SPIE) have been changed. The should both be set to 1\n");
#endif

	}

	reportTestExit(OMRPORTLIB, testName);
}


/*
 * Test protecting (and running) a function that does not fault
 *
 */
TEST(PortSigTest, sig_test1)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t result = 1;
	uint32_t protectResult;
	const char *testName = "omrsig_test1";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(OMRPORTLIB, testName);

	if (!omrsig_can_protect(0)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {

		/* test running a function which does not fault */
		handlerInfo.expectedType = 0;
		handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		handlerInfo.testName = testName;
		protectResult = omrsig_protect(
							sampleFunction, NULL,
							simpleHandlerFunction, &handlerInfo,
							0, /* protect from nothing */
							&result);

		if (protectResult != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 return value\n");
		}

		if (result != 8096) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- protected function did not execute correctly\n");
		}
	}

	reportTestExit(OMRPORTLIB, testName);
}

/*
 * Test protecting (and running) a function that raises a segv
 *
 */
TEST(PortSigTest, sig_test2)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t result = 1;
	uint32_t protectResult;
	uint32_t flags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_SIGSEGV;
	const char *testName = "omrsig_test2";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(OMRPORTLIB, testName);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {

		/* test running a function that raises a segv */
		handlerInfo.expectedType = OMRPORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		protectResult = omrsig_protect(
							raiseSEGV, (void *)testName,
							simpleHandlerFunction, &handlerInfo,
							flags,
							&result);

		if (protectResult != OMRPORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected OMRPORT_SIG_EXCEPTION_OCCURRED return value\n");
		}

		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}
	}

	reportTestExit(OMRPORTLIB, testName);
}

/*
 * Test protecting Function A against a SEGV with handler HA. A in turn protects Function B
 * against a SEGV with handler HB. Function B raises a sigsegv. HB should handle it, returning
 * OMRPORT_SIG_EXCEPTION_RETURN, control ends up after the call to omrsig_protect in B.
 * B then returns normally to A and then omrsigTest3.
 * A checks:
 * 	omrsig_protect returns OMRPORT_SIG_EXCEPTION_OCCURRED
 *  *result parameter to omrsig_protect call is 0 after call.
 *
 * 		->SEGV
 * 		|	 |
 * 		|	 |
 * 		|  	 -->HB
 * 		B		|
 *  	^   ---<-
 * 		|	|
 * 		A<--	HA
 * 		^
 * 		|
 * 		|
 *	omrsig_test3
 */
TEST(PortSigTest, sig_test3)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t result = 1;
	uint32_t protectResult;
	uint32_t flags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_SIGSEGV;
	const char *testName = "omrsig_test3";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(OMRPORTLIB, testName);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {
		handlerInfo.expectedType = 0;
		handlerInfo.returnValue = 0;
		handlerInfo.testName = testName;
		protectResult = omrsig_protect(
							nestedTestCatchSEGV, (void *)testName,
							simpleHandlerFunction, &handlerInfo,
							flags,
							&result);

		if (protectResult != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 return value\n");
		}

		if (result != 8096) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 8096 in *result\n");
		}
	}

	reportTestExit(OMRPORTLIB, testName);
}

/*
 * Test protecting Function A against a SEGV with handler HA. A in turn protects Function B against
 * nothing with handler HB. Function B raises a sigsegv. HB won't get invoked because it was not protecting against anything
 * so HA will be invoked, and return control to A (and then omrsig_test4).
 *
 * 		->SEGV--->--
 * 		|			|
 * 		|	 		|
 * 		B  	 	HB	|
 * 		^			|
 *  	|			|
 * 		|			|
 * 		A<------HA-<-
 * 		^
 * 		|
 *		|
 *	omrsig_test4
 */
TEST(PortSigTest, sig_test4)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t result = 1;
	uint32_t protectResult;
	uint32_t flags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_SIGSEGV;
	const char *testName = "omrsig_test4";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(OMRPORTLIB, testName);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {

		/* test running a function which does not fault */
		handlerInfo.expectedType = OMRPORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		protectResult = omrsig_protect(
							nestedTestCatchNothing, (void *)testName,
							simpleHandlerFunction, &handlerInfo,
							flags,
							&result);

		if (protectResult != OMRPORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected OMRPORT_SIG_EXCEPTION_OCCURRED return value\n");
		}

		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}
	}

	reportTestExit(OMRPORTLIB, testName);
}

/*
 * Test protecting Function A against a SEGV with handler HA. A in turn protects Function B
 * against a SEGV with handler HB. Function B raises a sigsegv. HB gets invoked, returns OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH
 * so that HA gets invoked, which then returns control to A (and then omrsig_test5).
 *
 * 		->SEGV
 * 		|	 |
 * 		|	 |
 * 		B  	 -->HB
 * 		^		|
 *  	|	    |
 * 		|       |
 * 		A<----<-HA
 * 		^
 * 		|
 * 		|
 * 	omrsig_test5
 *
 */
TEST(PortSigTest, sig_test5)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t result = 1;
	uint32_t protectResult;
	uint32_t flags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_SIGSEGV;
	const char *testName = "omrsig_test5";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(OMRPORTLIB, testName);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {

		handlerInfo.expectedType = OMRPORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		protectResult = omrsig_protect(
							nestedTestCatchAndIgnore, (void *)testName,
							simpleHandlerFunction, &handlerInfo,
							flags,
							&result);

		if (protectResult != OMRPORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected OMRPORT_SIG_EXCEPTION_OCCURRED return value\n");
		}

		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}
	}

	reportTestExit(OMRPORTLIB, testName);
}

/*
 * 		->SEGV->-
 * 		|	    |
 * 		|	    |
 * 		B  	    -->HB-->--SEGV->-
 * 		^		    			|
 *  	|		  				|
 *		|	 					|
 * 		|						|
 * 		|						|
 * 		A<---<--HA----<---------
 * 		^
 * 		|
 * 		|
 * 	omrsig_test6
 */
TEST(PortSigTest, sig_test6)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t result = 1;
	uint32_t protectResult;
	uint32_t flags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_SIGSEGV;
	const char *testName = "omrsig_test6";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(OMRPORTLIB, testName);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {

		handlerInfo.expectedType = OMRPORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		protectResult = omrsig_protect(
							nestedTestCatchAndCrash, (void *)testName,
							simpleHandlerFunction, &handlerInfo,
							flags,
							&result);

		if (protectResult != OMRPORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected OMRPORT_SIG_EXCEPTION_OCCURRED return value\n");
		}

		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}

#if 0
		/* CMVC 126838
		 * Verify that the current signal is still valid. There's no public API, so
		 * to run this test you need to port/unix/omrsignal.c (remove static from declaration of tlsKeyCurrentSignal)
		 * and add "<export name="tlsKeyCurrentSignal" />" to port/module.xml, to export tlsKeyCurrentSignal.
		 */
#if ! defined(WIN32)
		{
			extern omrthread_tls_key_t tlsKeyCurrentSignal;
			int signo = (int)(uintptr_t)omrthread_tls_get(omrthread_self(), tlsKeyCurrentSignal);
			int expectedSigno = 0;

			omrtty_printf("\n testing current signal\n");
			if (signo != expectedSigno) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "currentSignal corrupt -- got: %d expected: %d\n", signo, expectedSigno);
			}
		}
#endif
#endif

	}

	reportTestExit(OMRPORTLIB, testName);
}

/*
 * Test the exposure of the currently handled signal
 *
 */
TEST(PortSigTest, sig_test7)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t result = 1;
	uint32_t protectResult;
	uint32_t flags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_SIGSEGV;
	SigProtectHandlerInfo handlerInfo;
	const char *testName = "omrsig_test7";
	intptr_t currentSigNum = -1;

	handlerInfo.expectedType = OMRPORT_SIG_FLAG_SIGSEGV;
	handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_RETURN;
	handlerInfo.testName = testName;
	reportTestEntry(OMRPORTLIB, testName);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {
		currentSigNum = omrsig_get_current_signal();

		if (0 != currentSigNum) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 for current signal, received 0x%X\n", currentSigNum);
		}

		protectResult = omrsig_protect(
							raiseSEGV, (void *)testName,
							currentSigNumHandlerFunction, &handlerInfo,
							flags,
							&result);

		if (protectResult != OMRPORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected OMRPORT_SIG_EXCEPTION_OCCURRED return value\n");
		}

		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}

		currentSigNum = omrsig_get_current_signal();

		if (0 != currentSigNum) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 for current signal, received 0x%X\n", currentSigNum);
		}
	}

	reportTestExit(OMRPORTLIB, testName);
}

/*
 * Caller indicates that this may return OMRPORT_SIG_EXCEPTION_RETURN,
 * but we actually return OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH.
 */
static uintptr_t
callSigProtectWithContinueSearch(OMRPortLibrary *portLibrary, void *arg)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	SigProtectHandlerInfo handlerInfo;
	uint32_t protectResult;
	uint32_t flags = OMRPORT_SIG_FLAG_SIGSEGV | OMRPORT_SIG_FLAG_MAY_RETURN;
	const char *testName = "omrsig_test8";
	uintptr_t result = 1;

	handlerInfo.expectedType = OMRPORT_SIG_FLAG_SIGSEGV;
	handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH;
	handlerInfo.testName = testName;
	protectResult = omrsig_protect(
						nestedTestCatchAndContinueSearch, (void *)testName,
						simpleHandlerFunction, &handlerInfo,
						flags,
						&result);
	if (protectResult != OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH return value\n");
	}
	return result;
}

/*
 * Test handling of OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH.
 * invoke A with hander HA
 * 	invoke B with handler HB
 * 		invoke C with handler HC
 * 			C causes exception
 * 			HC returns OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH
 * 		HB returns OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH
 * 	HA returns OMRPORT_SIG_EXCEPTION_RETURN
 *
 */
TEST(PortSigTest, sig_test8)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	uintptr_t result = 1;
	uint32_t protectResult;
	uint32_t flags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_SIGSEGV;
	const char *testName = "omrsig_test8";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(OMRPORTLIB, testName);

	if (!omrsig_can_protect(flags)) {
		operationNotSupported(OMRPORTLIB, "the specified signals are");
	} else {

		handlerInfo.expectedType = OMRPORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = OMRPORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		protectResult = omrsig_protect(
							callSigProtectWithContinueSearch, (void *)testName,
							simpleHandlerFunction, &handlerInfo,
							flags,
							&result);
		outputComment(OMRPORTLIB, "omrsig_test8 protectResult=%d\n", protectResult);
#if defined(WIN64)
		if (protectResult != OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH in protectResult\n");
		}
#else
		if (protectResult != OMRPORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH in protectResult\n");
		}
#endif
		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}
	}
	reportTestExit(OMRPORTLIB, testName);
}


#if defined(J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS)
/*
 * Tests the async signals to verify that our handler is called both when the signal is raised and injected by another process
 *
 * To test that the handler was called, omrsig_test_async_unix_handler sets controlFlag to 0, then expects the handler to have set it to 1
 *
 * The child process is another instance of pltest with the argument -child_omrsig_injectSignal_<PID>_<signal to inject>.
 * In the child process:
 * 	- main.c peels off -child_
 * 	- omrsig_run_tests looks for omrsig_injectSignal, and if found, reads the PID and signal number and injects it
 *
 * NOTE : Assumes 0.5 seconds is long enough for a child process to be kicked off, inject a signal and have our handler kick in
 * */
TEST(PortSigTest, sig_test_async_unix_handler)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrsig_test_async_unix_handler";
	AsyncHandlerInfo handlerInfo;
	uint32_t setAsyncRC;
	unsigned int index;
	omrthread_monitor_t asyncMonitor;
	intptr_t monitorRC;
	int pid = getpid();

	reportTestEntry(OMRPORTLIB, testName);
	omrtty_printf("\tpid: %i\n", pid);

	handlerInfo.testName = testName;

	monitorRC = omrthread_monitor_init_with_name(&asyncMonitor, 0, "omrsignalTest_async_monitor");

	if (0 != monitorRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrthread_monitor_init_with_name failed with %i\n", monitorRC);
		FAIL();
	}

	handlerInfo.monitor = &asyncMonitor;

	setAsyncRC = omrsig_set_async_signal_handler(asyncTestHandler, &handlerInfo, OMRPORT_SIG_FLAG_SIGQUIT | OMRPORT_SIG_FLAG_SIGABRT | OMRPORT_SIG_FLAG_SIGTERM | OMRPORT_SIG_FLAG_SIGINT);
	if (setAsyncRC == (uint32_t)OMRPORT_SIG_ERROR) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrsig_set_async_signal_handler returned: OMRPORT_SIG_ERROR\n");
		goto exit;
	}

#if defined(AIXPPC)
	setAsyncRC = omrsig_set_async_signal_handler(asyncTestHandler, &handlerInfo, OMRPORT_SIG_FLAG_SIGRECONFIG);
	if (setAsyncRC == OMRPORT_SIG_ERROR) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrsig_set_async_signal_handler returned: OMRPORT_SIG_ERROR\n");
		goto exit;
	}
#endif

	/* iterate through all the known signals */
	for (index = 0; index < sizeof(testSignalMap) / sizeof(testSignalMap[0]); index++) {
		OMRProcessHandle childProcess = NULL;
		char options[SIG_TEST_SIZE_EXENAME];
		int signum = testSignalMap[index].unixSignalNo;

		handlerInfo.expectedType = testSignalMap[index].portLibSignalNo;


		omrtty_printf("\n\tTesting %s\n", testSignalMap[index].unixSignalString);

		/* asyncTestHandler notifies the monitor once it has set controlFlag to 0; */
		omrthread_monitor_enter(asyncMonitor);

		/* asyncTestHandler will change controlFlag to 1 */
		handlerInfo.controlFlag = 0;


		/* test that we handle the signal when it is raise()'d */
		portTestOptionsGlobal = omrsig_get_options();
		if (0 != (OMRPORT_SIG_OPTIONS_ZOS_USE_CEEHDLR & portTestOptionsGlobal)) {
#if defined(J9ZOS390)
			/* This prevents LE from sending the corresponding LE condition to our thread */
			sighold(signum);
			raise(signum);
			sigrelse(signum);
#endif
		} else {
			raise(signum);
		}

		/* we don't want to hang - if we haven't been notified in 20 seconds it doesn't work */
		(void)omrthread_monitor_wait_timed(asyncMonitor, 20000, 0);

		if (1 != handlerInfo.controlFlag) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s handler was not invoked when %s was raised\n", testSignalMap[index].portLibSignalString, testSignalMap[index].unixSignalString);
		}

		handlerInfo.controlFlag = 0;
		omrthread_monitor_exit(asyncMonitor);

		/* Test that we handle the signal when it is injected */

		/* build the pid and signal number in the form "-child_omrsig_injectSignal_<PID>_<signal>" */
		omrstr_printf(options, SIG_TEST_SIZE_EXENAME, "-child_omrsig_injectSignal_%i_%i", pid, signum);
		omrtty_printf("\t\tlaunching child process with arg %s\n", options);

		omrthread_monitor_enter(asyncMonitor);

		childProcess = launchChildProcess(OMRPORTLIB, testName, portTestEnv->_argv[0], options);

		if (NULL == childProcess) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot launch test process! Can not perform test!");
			omrthread_monitor_exit(asyncMonitor);
			goto exit;
		}

		/* we don't want to hang - if we haven't been notified in 20 seconds it doesn't work */
		(void)omrthread_monitor_wait_timed(asyncMonitor, 20000, 0);

		if (1 != handlerInfo.controlFlag) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s handler was NOT invoked when %s was injected by another process\n", testSignalMap[index].portLibSignalString, testSignalMap[index].unixSignalString);
		}

		omrthread_monitor_exit(asyncMonitor);

		j9process_close(OMRPORTLIB, &childProcess, 0);

#if defined(J9ZOS390)
		/*
		 * Verify that async signals can't be received by the asyncSignalReporter thread.
		 * pltest runs with 2 threads only, this thread and the asyncSignalReporter thread.
		 * We will mask the signal on this thread, inject it via kill, and verify that it does
		 * not get received.
		 */
		omrtty_printf("\t\tmasking signal %i on this thread\n", signum);
		sighold(signum);

		/* build the pid and signal number in the form "-child_omrsig_injectSignal_<PID>_<signal>_unhandled" */
		omrstr_printf(options, SIG_TEST_SIZE_EXENAME, "-child_omrsig_injectSignal_%i_%i_unhandled", pid, signum);
		omrtty_printf("\t\tlaunching child process with arg %s\n", options);

		omrthread_monitor_enter(asyncMonitor);
		handlerInfo.controlFlag = 0;

		childProcess = launchChildProcess(OMRPORTLIB, testName, portTestEnv->_argv[0], options);

		if (NULL == childProcess) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot launch test process! Can not perform test!");
			omrthread_monitor_exit(asyncMonitor);
			goto exit;
		}

		/* wait 5 seconds for the signal to be received */
		(void)omrthread_monitor_wait_timed(asyncMonitor, 5000, 0);

		/* verify that the signal was NOT received */
		if (0 != handlerInfo.controlFlag) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s handler was unexpectedly invoked when %s was injected by another process\n",
							   testSignalMap[index].portLibSignalString, testSignalMap[index].unixSignalString);
		}

		/* unblock the signal and handle it */
		omrtty_printf("\t\tunmasking signal %i on this thread\n", signum);
		sigrelse(signum);
		(void)omrthread_monitor_wait_timed(asyncMonitor, 20000, 0);
		if (1 != handlerInfo.controlFlag) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s handler was NOT invoked when %s was injected by another process\n", testSignalMap[index].portLibSignalString, testSignalMap[index].unixSignalString);
		}

		omrthread_monitor_exit(asyncMonitor);

		j9process_close(OMRPORTLIB, &childProcess, 0);
#endif

	} /* for */

exit:
	omrsig_set_async_signal_handler(asyncTestHandler, &handlerInfo, 0);
	omrthread_monitor_destroy(asyncMonitor);
	reportTestExit(OMRPORTLIB, testName);

}
#endif /* J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS */


/*
 *
 *
 *		C->-SEGV->-HC--->---|
 * 		|					|
   		------<---<-|		|
        			|		|
        			|		|
 * 		->SEGV->-   |		|
 * 		|	    |   |		|
 * 		|	    |	|		|
 * 		B  	    -->HB--<-----
 * 		^			|
 *  	|			|
 *		|	 		|
 * 		|	--<-----|
 * 		|	|
 * 		A<--- 	   HA
 * 		^
 * 		|
 * 		|
 * 	omrsigTest<future>
 */

