/*******************************************************************************
 * Copyright (c) 2015, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if defined(J9ZOS390)
#define _OPEN_THREADS 2
#define _UNIX03_SOURCE
#endif /* defined(J9ZOS390) */

#if !defined(OMR_OS_WINDOWS)
#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <dlfcn.h>
#endif /* !defined(OMR_OS_WINDOWS) */
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#if !defined(OMR_OS_WINDOWS)
#include <unistd.h>
#endif /* !defined(OMR_OS_WINDOWS) */

#include "omrthread.h"
#include "omrTest.h"
#include "omr.h"
#include "omrTestHelpers.h"
#include "omrvm.h"
#include "omrsig.h"

#include "sigTestHelpers.hpp"

enum TestAction {
#if !defined(OMR_OS_WINDOWS)
	test_sigaction,
#endif /* !defined(OMR_OS_WINDOWS) */
	test_signal,
	test_omrsig_handler,
	test_omrsig_primary_signal,
#if !defined(OMR_OS_WINDOWS)
	test_omrsig_primary_sigaction
#endif /* !defined(OMR_OS_WINDOWS) */
};

#if !defined(PRId64)
#if defined(OMR_OS_WINDOWS) || defined(J9ZOS390)
#define PRId64 "I64d"
#else
#error no value for PRId64
#endif /* defined(OMR_OS_WINDOWS) || defined(J9ZOS390) */
#endif

/* Windows does not have sigsetjmp, siglongjump, or sigjmp_buf */
#if defined(OMR_OS_WINDOWS)
#define sigsetjmp(env, savesigs) setjmp(env)
#define siglongjmp(env, val) longjmp(env, val)
typedef jmp_buf sigjmp_buf;
#endif /* defined(OMR_OS_WINDOWS) */

static sigjmp_buf env;
static volatile int handlerCalls;
static void handlerPrimary(int sig);
static void handlerPrimaryInstaller(int sig);
static void handlerSecondary(int sig);
static void handlerSecondaryInstaller(int sig);
static void handlerTertiaryInstaller(int sig);
#if defined(OMR_OS_WINDOWS)
static int signumOptions[] = {SIGABRT, 10000};
#define NUM_TEST_CONDITIONS (2*2*4*2)
#else /* defined(OMR_OS_WINDOWS) */
static int signalRaisingThread(void *entryArg);
static volatile bool primaryMasked;
static volatile bool secondaryMasked;
static int signumOptions[] = {
#if defined(AIXPPC)
	/* On AIX, calls to sigaltstack() fail after siglongjmp out of a handler,
	 * so cannot test SIGABRT effectively.
	 */
	SIGURG,
#else /* defined(AIXPPC) */
	SIGABRT,
#endif /* defined(AIXPPC) */
	10000, SIGCHLD, SIGCONT
};
#define THREADING_TEST_ITERATIONS 200
#define NUM_TEST_CONDITIONS (2*2*4*2*4*(2*2*2*2))
static void handlerPrimaryInfo(int sig, siginfo_t *siginfo, void *uc);
static void handlerSecondaryInfo(int sig, siginfo_t *siginfo, void *uc);
static unsigned int flagOptions[] = {SA_SIGINFO, SA_RESETHAND, SA_NODEFER, SA_ONSTACK};
typedef int (*SIGACTION)(int signum, const struct sigaction *act, struct sigaction *oldact);
#if defined(J9ZOS390)
#pragma map (sigactionOS, "\174\174SIGACT")
extern int sigactionOS(int, const struct sigaction *, struct sigaction *);
#else
static SIGACTION sigactionOS = NULL;
#endif /* defined(J9ZOS390) */
#endif /* defined(OMR_OS_WINDOWS) */
static sighandler_t handlerOptions[] = {SIG_DFL, SIG_IGN, NULL, handlerPrimary, handlerSecondary};

static omr_error_t test(TestAction testFunc);
static omr_error_t setupExistingHandlerConditions(bool existingPrimary, bool existingSecondary, bool onStack, int signum);
static bool checkPreviousHandler(int signum, TestAction testFunc, sighandler_t expectedPreviousHandler, sighandler_t previousHandler);
static omr_error_t performTestAndCheck(int expectedHandlerCalls, bool onStackCondition, bool testingSecondaryHandler, int signum);
#if defined(OMR_OS_WINDOWS)
static omr_error_t runTest(bool existingPrimary, bool existingSecondary, sighandler_t action, int signum, TestAction testFunc);
#else /* defined(OMR_OS_WINDOWS) */
static void checkSignalMask(int signum, bool expected, bool checkingPrimary);
typedef void (*sigaction_t)(int sig, siginfo_t *siginfo, void *uc);
static omr_error_t runTest(bool existingPrimary, bool existingSecondary, struct sigaction *action, bool onStack, int signum, TestAction testFunc);
static omr_error_t runSigactionTest(int *expectedHandlerCalls, bool existingPrimary, bool existingSecondary,
									int signum, struct sigaction *act, sighandler_t *expectedPreviousHandler, sighandler_t *previousHandler);
static omr_error_t runPrimarySigactionTest(int *expectedHandlerCalls, bool existingPrimary, bool existingSecondary,
		int signum, struct sigaction *act, sighandler_t *expectedPreviousHandler, sighandler_t *previousHandler);
#endif /* defined(OMR_OS_WINDOWS) */
static omr_error_t runSignalTest(int *expectedHandlerCalls, bool existingPrimary, bool existingSecondary,
								 int signum, sighandler_t action, sighandler_t *expectedPreviousHandler, sighandler_t *previousHandler);
static omr_error_t runHandlerTest(int *expectedHandlerCalls, bool existingPrimary, bool existingSecondary,
								  int signum, sighandler_t *expectedPreviousHandler, sighandler_t *previousHandler);
static omr_error_t runPrimarySignalTest(int *expectedHandlerCalls, bool existingPrimary, bool existingSecondary,
										int signum, sighandler_t action, sighandler_t *expectedPreviousHandler, sighandler_t *previousHandler);

#if !defined(OMR_OS_WINDOWS)
TEST(OmrSigTest, sigactionTest)
{
	EXPECT_TRUE(OMR_ERROR_NONE == test(test_sigaction)) << "sigaction() test failed.";
}
#endif /* !defined(OMR_OS_WINDOWS) */

TEST(OmrSigTest, signalTest)
{
	EXPECT_TRUE(OMR_ERROR_NONE == test(test_signal)) << "signal() test failed.";
}

TEST(OmrSigTest, omrsig_handlerTest)
{
	EXPECT_TRUE(OMR_ERROR_NONE == test(test_omrsig_handler)) << "omrsig_handler() test failed.";
}

TEST(OmrSigTest, omrsig_primary_signalTest)
{
	EXPECT_TRUE(OMR_ERROR_NONE == test(test_omrsig_primary_signal)) << "omrsig_primary_signal() test failed.";
}

#if !defined(OMR_OS_WINDOWS)
TEST(OmrSigTest, omrsig_primary_sigactionTest)
{
	EXPECT_TRUE(OMR_ERROR_NONE == test(test_omrsig_primary_sigaction)) << "omrsig_primary_sigaction() test failed.";
}

TEST(OmrSigTest, omrsigReentrancyTest)
{
	/* Install handler while another thread raises signals, both to itself and the main thread. */
	int testSignals[] = {SIGABRT, SIGCHLD};
	struct sigaction act = {{0}};
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	omrthread_t thread = NULL;
	primaryMasked = true;
	secondaryMasked = true;

	for (int i = 0; i < 2; i += 1) {
		int signum = testSignals[i];
		act.sa_handler = handlerPrimary;
		omrsig_primary_sigaction(signum, &act, NULL);
		act.sa_handler = handlerSecondary;
		sigaction(signum, &act, NULL);
		handlerCalls = 0;

		/* Raise signals while installing a primary handler. */
		act.sa_handler = handlerPrimary;
		ASSERT_NO_FATAL_FAILURE(createThread(&thread, FALSE, J9THREAD_CREATE_JOINABLE, signalRaisingThread, &signum));
		for (int j = 0; j < THREADING_TEST_ITERATIONS * 10; j += 1) {
			omrsig_primary_sigaction(signum, &act, NULL);
		}
		EXPECT_TRUE(J9THREAD_SUCCESS == omrthread_join(thread))
				<< "omrthread_join() failed.";

		EXPECT_TRUE(handlerCalls > 0)
				<< "Expected handler to be called for primary test, got " << handlerCalls << " calls.";
		handlerCalls = 0;

		/* Raise signals while installing a secondary handler. */
		act.sa_handler = handlerSecondary;
		ASSERT_NO_FATAL_FAILURE(createThread(&thread, FALSE, J9THREAD_CREATE_JOINABLE, signalRaisingThread, &signum));
		for (int j = 0; j < THREADING_TEST_ITERATIONS * 10; j += 1) {
			sigaction(signum, &act, NULL);
		}
		EXPECT_TRUE(J9THREAD_SUCCESS == omrthread_join(thread))
				<< "omrthread_join() failed.";

		/* Check handler calls. */
		EXPECT_TRUE(handlerCalls > 0)
				<< "Expected handler to be called for secondary test, got " << handlerCalls << " calls.";
	}
}

#endif /* !defined(OMR_OS_WINDOWS) */

TEST(OmrSigTest, handlerInstallingHandlerTest)
{
	/* Test using a signal handler which installs a handler and raises a signal. */
#if !defined(OMR_OS_WINDOWS)
	sigset_t mask;
	sigemptyset(&mask);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);
#endif /* defined(OMR_OS_WINDOWS) */

	omrsig_primary_signal(SIGABRT, handlerPrimaryInstaller);
	signal(SIGABRT, handlerSecondaryInstaller);

	handlerCalls = sigsetjmp(env, 1);
	if (0 == handlerCalls) {
		raise(SIGABRT);
	}

	EXPECT_TRUE(handlerCalls == 4)
			<< "Expected 4 handler calls, got " << handlerCalls << ".";
}

#if !defined(OMR_OS_WINDOWS)
static int
signalRaisingThread(void *entryArg)
{
	int signum = *(int *)entryArg;
	if (SIGABRT != signum) {
		sigset_t mask;
		sigfillset(&mask);
		pthread_sigmask(SIG_SETMASK, &mask, NULL);
	}
	for (int i = 0; i < THREADING_TEST_ITERATIONS; i += 1) {
		if (SIGABRT == signum) {
			/* Raise signal to this thread. */
			int newHandlerCalls = sigsetjmp(env, 1);
			if (0 == newHandlerCalls) {
				pthread_kill(pthread_self(), signum);
			}
			handlerCalls = newHandlerCalls;
		} else {
			/* Raise signal to another thread. */
			kill(getpid(), signum);
		}
	}
	return 0;
}
#endif /* !defined(OMR_OS_WINDOWS) */

/* Iterate through all possible test conditions and run the tests. */
static omr_error_t
test(TestAction testFunc)
{
	omr_error_t rc = OMR_ERROR_NONE;
	for (int i = 0; (i < NUM_TEST_CONDITIONS) && (OMR_ERROR_NONE == rc); i += 1) {
		/* The bits in i define the test conditions:
		 * 0000 0000 0000
		 *     -    -   |	Existing primary handler
		 *     -    -  | 	Existing secondary handler
		 *     -    -||  	Registering SIG_DFL, SIG_IGN, NULL, or a handler function.
		 *     -  ||-    	Signal num to test
		 *     - |  -    	Alternate signal stack set.
		 *  |||-|   -    	Flags SA_SIGINFO, SA_RESETHAND, SA_NODEFER, SA_ONSTACK
		 */

		/* Select handler function. */
		int handlerIndex = ((i & 12) >> 2);
		if ((3 == handlerIndex)
#if !defined(OMR_OS_WINDOWS)
			&& (testFunc != test_omrsig_primary_sigaction)
#endif /* !defined(OMR_OS_WINDOWS) */
			&& (testFunc != test_omrsig_primary_signal)) {
			handlerIndex += 1;
		}

#if !defined(OMR_OS_WINDOWS)
		struct sigaction act = {{0}};
		act.sa_flags = 0;
		if ((test_sigaction == testFunc) || (test_omrsig_primary_sigaction == testFunc)) {
			/* Compose flags. */
			for (int j = 0; j < 4; ++ j) {
				act.sa_flags |= ((i >> (7 + j)) & 1) * flagOptions[j];
			}

			/* Create handler struct. */
			if ((SA_SIGINFO & act.sa_flags) && (3 == handlerIndex)) {
				act.sa_sigaction = handlerPrimaryInfo;
			} else if ((SA_SIGINFO & act.sa_flags) && (4 == handlerIndex)) {
				act.sa_sigaction = handlerSecondaryInfo;
			} else {
				act.sa_handler = handlerOptions[handlerIndex];
			}
		} else {
			act.sa_handler = handlerOptions[handlerIndex];
		}
		sigemptyset(&act.sa_mask);
#endif /* defined(OMR_OS_WINDOWS) */

		int signum = signumOptions[(i & 48) >> 4];
		bool onStack = (i & 64) != 0;
		bool existingPrimary = (i & 1) != 0;
		bool existingSecondary = (i & 2) != 0;

		if (OMR_ERROR_NONE != runTest(
				existingPrimary,
				existingSecondary,
#if defined(OMR_OS_WINDOWS)
				handlerOptions[handlerIndex],
#else /* defined(OMR_OS_WINDOWS) */
				&act,
				onStack,
#endif /* defined(OMR_OS_WINDOWS) */
				signum,
				testFunc)) {

			int64_t flags = 0;
#if !defined(OMR_OS_WINDOWS)
			flags = act.sa_flags;
#endif /* !defined(OMR_OS_WINDOWS) */
			rc = OMR_ERROR_INTERNAL;

			printf("Test %d:%d failed at %s:%d. Conditions: %sexisting primary, %sexisting secondary, flags = %" PRId64 ", signum is %d, handler is 0x%p, %salt signal stack.\n",
				   testFunc, i,
				   __FILE__, __LINE__,
				   existingPrimary ? "" : "no ",
				   existingSecondary ? "" : "no ",
				   flags,
				   signum,
				   handlerOptions[handlerIndex],
				   onStack ? "" : "no ");
		}
	}
	return rc;
}

/* Setup the specified test conditions, run the test, and check the result. */
#if defined(OMR_OS_WINDOWS)
static omr_error_t
runTest(bool existingPrimary, bool existingSecondary, sighandler_t act, int signum, TestAction testFunc)
#else /* defined(OMR_OS_WINDOWS) */
static omr_error_t
runTest(bool existingPrimary, bool existingSecondary, struct sigaction *act, bool onStack, int signum, TestAction testFunc)
#endif /* defined(OMR_OS_WINDOWS) */
{
	omr_error_t rc = OMR_ERROR_NONE;
	int expectedHandlerCalls = 0;
	handlerCalls = 0;
	sighandler_t expectedPreviousHandler = SIG_DFL;
	sighandler_t previousHandler = SIG_DFL;
#if defined(OMR_OS_WINDOWS)
	sighandler_t action = act;
	bool onStack = false;
#else /* defined(OMR_OS_WINDOWS) */
	sighandler_t action = act->sa_handler;
	primaryMasked = false;
	secondaryMasked = false;
#endif /* defined(OMR_OS_WINDOWS) */

	if (signum != signumOptions[1]) {
		rc = setupExistingHandlerConditions(existingPrimary, existingSecondary, onStack, signum);
	}

	/* Calculate the expected number of signal handler calls and call the test function. */
	if (OMR_ERROR_NONE == rc) {
		switch (testFunc) {
#if !defined(OMR_OS_WINDOWS)
		case test_sigaction:
			rc = runSigactionTest(&expectedHandlerCalls, existingPrimary, existingSecondary, signum, act, &expectedPreviousHandler, &previousHandler);
			break;
#endif /* !defined(OMR_OS_WINDOWS) */
		case test_signal:
			rc = runSignalTest(&expectedHandlerCalls, existingPrimary, existingSecondary, signum, action, &expectedPreviousHandler, &previousHandler);
			break;
		case test_omrsig_handler:
			rc = runHandlerTest(&expectedHandlerCalls, existingPrimary, existingSecondary, signum, &expectedPreviousHandler, &previousHandler);
			break;
		case test_omrsig_primary_signal:
			rc = runPrimarySignalTest(&expectedHandlerCalls, existingPrimary, existingSecondary, signum, action, &expectedPreviousHandler, &previousHandler);
			break;
#if !defined(OMR_OS_WINDOWS)
		case test_omrsig_primary_sigaction:
			rc = runPrimarySigactionTest(&expectedHandlerCalls, existingPrimary, existingSecondary, signum, act, &expectedPreviousHandler, &previousHandler);
			break;
#endif /* !defined(OMR_OS_WINDOWS) */
		}
	}
	if (OMR_ERROR_NONE == rc) {
		if (!checkPreviousHandler(signum, testFunc, expectedPreviousHandler, previousHandler)) {
			rc = OMR_ERROR_INTERNAL;
			printf("Incorrect returned previous handler. Found 0x%p, expected 0x%p at %s:%d.\n",
				   previousHandler, expectedPreviousHandler, __FILE__, __LINE__);
		}
	}

	if (OMR_ERROR_NONE == rc) {
		if (signum == signumOptions[1]) {
			expectedHandlerCalls = 0;
		}
		rc = performTestAndCheck(
				 expectedHandlerCalls,
#if defined(OMR_OS_WINDOWS)
				 false,
				 false,
#else /* defined(OMR_OS_WINDOWS) */
				 onStack && (SA_ONSTACK & act->sa_flags) && handlerIsFunction(action)
				 && ((testFunc == test_sigaction) || (testFunc == test_omrsig_primary_sigaction)),
				 testFunc == test_sigaction,
#endif /* defined(OMR_OS_WINDOWS) */
				 signum);
	}

#if !defined(OMR_OS_WINDOWS)
	/* Cleanup alternate stack if it exists. */
	if (onStack && (signum != signumOptions[1])) {
		stack_t stack;
		sigaltstack(NULL, &stack);
		free(stack.ss_sp);
	}
#endif /* !defined(OMR_OS_WINDOWS) */

	return rc;
}

/* Set existing primary handler, secondary handler, and/or on stack condition before
 * running the test.
 */
static omr_error_t
setupExistingHandlerConditions(bool existingPrimary, bool existingSecondary, bool onStack, int signum)
{
	omr_error_t rc = OMR_ERROR_NONE;

	/* First, remove existing handlers. */
	if (SIG_ERR == omrsig_primary_signal(signum, SIG_DFL)) {
		rc = OMR_ERROR_INTERNAL;
		printf("Setting primary handler to SIG_DFL before setting test conditions failed at %s:%d.\n", __FILE__, __LINE__);
	}
	if (OMR_ERROR_NONE == rc) {
		if (SIG_ERR == signal(signum, SIG_DFL)) {
			rc = OMR_ERROR_INTERNAL;
			printf("Setting secondary handler to SIG_DFL before setting test conditions failed at %s:%d.\n", __FILE__, __LINE__);
		}
	}

	/* Setup existing primary handler condition. */
	if (OMR_ERROR_NONE == rc) {
		if (existingPrimary) {
#if defined(OMR_OS_WINDOWS)
			if (SIG_ERR == omrsig_primary_signal(signum, handlerPrimary)) {
				rc = OMR_ERROR_INTERNAL;
			}
#else
			struct sigaction act = {{0}};
			act.sa_handler = handlerPrimary;
			act.sa_flags = 0;
			sigemptyset(&act.sa_mask);
			if (0 != omrsig_primary_sigaction(signum, &act, NULL)) {
				rc = OMR_ERROR_INTERNAL;
			}
#endif /* defined(OMR_OS_WINDOWS) */
		} else {
			if (SIG_ERR == omrsig_primary_signal(signum, SIG_IGN)) {
				rc = OMR_ERROR_INTERNAL;
			}
		}
		if (OMR_ERROR_NONE != rc) {
			printf("Installing existing primary handler condition failed at %s:%d.\n", __FILE__, __LINE__);
		}
	}

	/* Setup existing secondary handler condition. */
	if (OMR_ERROR_NONE == rc) {
		if (existingSecondary) {
			if (SIG_ERR == signal(signum, handlerSecondary)) {
				rc = OMR_ERROR_INTERNAL;
			}
		} else {
			if (SIG_ERR == signal(signum, SIG_IGN)) {
				rc = OMR_ERROR_INTERNAL;
			}
		}
		if (OMR_ERROR_NONE != rc) {
			printf("Installing existing secondary handler condition failed at %s:%d.\n", __FILE__, __LINE__);
		}
	}

#if !defined(OMR_OS_WINDOWS)
	/* Setup signal stack condition. */
	if (OMR_ERROR_NONE == rc) {
		stack_t stack;
		if (onStack) {
			stack.ss_sp = malloc(SIGSTKSZ);
			stack.ss_size = SIGSTKSZ;
			stack.ss_flags = 0;
		} else {
			stack.ss_flags = SS_DISABLE;
			/* To compensate for OSX bug, this is needed to prevent ENOMEM error. */
			stack.ss_size = SIGSTKSZ;
			stack.ss_sp = NULL;
		}
		if (0 != sigaltstack(&stack, NULL)) {
			rc = OMR_ERROR_INTERNAL;
			printf("sigaltstack() failed with error code %d in setting test conditions at %s:%d.\n", errno, __FILE__, __LINE__);
		}
	}
#endif /* !defined(OMR_OS_WINDOWS) */

	handlerCalls = 0;

	return rc;
}

#if !defined(OMR_OS_WINDOWS)
static omr_error_t
runPrimarySigactionTest(int *expectedHandlerCalls, bool existingPrimary, bool existingSecondary,
						int signum, struct sigaction *act, sighandler_t *expectedPreviousHandler, sighandler_t *previousHandler)
{
	omr_error_t rc = OMR_ERROR_NONE;
	struct sigaction oldact = {{0}};

	*expectedHandlerCalls = 2;
	if (existingSecondary) {
		*expectedHandlerCalls += 1;
	}
	if (!handlerIsFunction(act)) {
		*expectedHandlerCalls = existingSecondary ? 1 : 0;
	}
	if ((0 == omrsig_primary_sigaction(signum, act, &oldact)) == (signum == signumOptions[1])) {
		rc = OMR_ERROR_INTERNAL;
		printf("omrsig_primary_sigaction() unexpectedly returned %d, expected %d at %s:%d.\n",
			   (signum == signumOptions[1]) ? 0 : -1,
			   (signum == signumOptions[1]) ? -1 : 0,
			   __FILE__, __LINE__);
	}
	*previousHandler = oldact.sa_handler;
	if (existingPrimary) {
		*expectedPreviousHandler = handlerPrimary;
	} else {
		*expectedPreviousHandler = SIG_IGN;
	}

	primaryMasked = !(act->sa_flags & SA_NODEFER) && handlerIsFunction(act);
	secondaryMasked = false;
	return rc;
}
#endif /* !defined(OMR_OS_WINDOWS) */

static omr_error_t
runPrimarySignalTest(int *expectedHandlerCalls, bool existingPrimary, bool existingSecondary,
					 int signum, sighandler_t action, sighandler_t *expectedPreviousHandler, sighandler_t *previousHandler)
{
#if defined(OMR_OS_WINDOWS)
	*expectedHandlerCalls = 1;
#else /* defined(OMR_OS_WINDOWS) */
	*expectedHandlerCalls = 2;
#endif /* defined(OMR_OS_WINDOWS) */
	if (existingSecondary) {
		*expectedHandlerCalls += 1;
	}
	if (!handlerIsFunction(action)) {
		*expectedHandlerCalls = existingSecondary ? 1 : 0;
	}
	*previousHandler = omrsig_primary_signal(signum, action);
	if (existingPrimary) {
		*expectedPreviousHandler = handlerPrimary;
	} else if (signum == signumOptions[1]) {
		*expectedPreviousHandler = SIG_ERR;
	} else {
		*expectedPreviousHandler = SIG_IGN;
	}
	return OMR_ERROR_NONE;
}

static omr_error_t
runHandlerTest(int *expectedHandlerCalls, bool existingPrimary, bool existingSecondary,
			   int signum, sighandler_t *expectedPreviousHandler, sighandler_t *previousHandler)
{
	omr_error_t rc = OMR_ERROR_NONE;
	*expectedHandlerCalls = 0;
#if defined(OMR_OS_WINDOWS)
	/* On Windows, primary is reset after first signal, but jump from handler prevents the
	 * reset of the secondary when it is called here.
	 */
	if (existingPrimary) {
		*expectedHandlerCalls = 1;
	}
	if (existingSecondary) {
		*expectedHandlerCalls += 2;
	}
#else /* !defined(OMR_OS_WINDOWS) */
	if (existingPrimary) {
		*expectedHandlerCalls = 2;
	}
	if (existingSecondary) {
		*expectedHandlerCalls += 1;
	}
#endif /* defined(OMR_OS_WINDOWS) */
	if (0 == sigsetjmp(env,0) && (signum != signumOptions[1])) {
		int ret = omrsig_handler(signum, NULL, NULL);
		if ((OMRSIG_RC_SIGNAL_HANDLED != ret)
			== ((signum != signumOptions[1]) && existingSecondary)) {
			rc = OMR_ERROR_INTERNAL;
			printf("omrsig_handler() unexpectedly returned %s, expected %s at %s:%d.\n",
				   (OMRSIG_RC_SIGNAL_HANDLED == ret) ? "OMRSIG_RC_SIGNAL_HANDLED" : "OMRSIG_RC_DEFAULT_ACTION_REQUIRED",
				   (OMRSIG_RC_SIGNAL_HANDLED == ret) ? "OMRSIG_RC_DEFAULT_ACTION_REQUIRED" : "OMRSIG_RC_SIGNAL_HANDLED",
				   __FILE__, __LINE__);
		}
	}
	*previousHandler = SIG_DFL;
#if !defined(OMR_OS_WINDOWS)
	primaryMasked = existingPrimary;
#endif /* !defined(OMR_OS_WINDOWS) */
	return rc;
}

#if !defined(OMR_OS_WINDOWS)
static omr_error_t
runSigactionTest(int *expectedHandlerCalls, bool existingPrimary, bool existingSecondary,
				 int signum, struct sigaction *act, sighandler_t *expectedPreviousHandler, sighandler_t *previousHandler)
{
	omr_error_t rc = OMR_ERROR_NONE;
	struct sigaction oldact = {{0}};

	*expectedHandlerCalls = 0;
	if (existingPrimary) {
		*expectedHandlerCalls += 2;
	}
	if (handlerIsFunction(act)) {
		*expectedHandlerCalls += 2;
		if (act->sa_flags & SA_RESETHAND) {
			*expectedHandlerCalls -= 1;
		}
	}
	if ((0 == sigaction(signum, act, &oldact)) == (signum == signumOptions[1])) {
		rc = OMR_ERROR_INTERNAL;
		printf("omrsig_primary_sigaction() unexpectedly returned %d, expected %d at %s:%d.\n",
			   (signum == signumOptions[1]) ? 0 : -1,
			   (signum == signumOptions[1]) ? -1 : 0,
			   __FILE__, __LINE__);
	}
	*previousHandler = oldact.sa_handler;
	if (existingSecondary) {
		*expectedPreviousHandler = handlerSecondary;
	} else {
		*expectedPreviousHandler = SIG_IGN;
	}
	primaryMasked = existingPrimary;
	secondaryMasked = (!(act->sa_flags & SA_NODEFER)
#if (defined(AIXPPC) || defined(J9ZOS390))
					   && !(act->sa_flags & SA_RESETHAND)
#endif /* (defined(AIXPPC) || defined(J9ZOS390)) */
					   && handlerIsFunction(act));
	return rc;
}
#endif /* !defined(OMR_OS_WINDOWS) */

static omr_error_t
runSignalTest(int *expectedHandlerCalls, bool existingPrimary, bool existingSecondary,
			  int signum, sighandler_t action, sighandler_t *expectedPreviousHandler, sighandler_t *previousHandler)
{
	*expectedHandlerCalls = 1;
	if (existingPrimary) {
#if defined(OMR_OS_WINDOWS)
		*expectedHandlerCalls = 2;
#else /* defined(OMR_OS_WINDOWS) */
		*expectedHandlerCalls = 3;
#endif /* defined(OMR_OS_WINDOWS) */
	}
	if (!handlerIsFunction(action)) {
#if defined(OMR_OS_WINDOWS)
		*expectedHandlerCalls = existingPrimary ? 1 : 0;
#else /* defined(OMR_OS_WINDOWS) */
		*expectedHandlerCalls = existingPrimary ? 2 : 0;
#endif /* defined(OMR_OS_WINDOWS) */
	}
	*previousHandler = signal(signum, action);
	if (existingSecondary) {
		*expectedPreviousHandler = handlerSecondary;
	} else if (signum == signumOptions[1]) {
		*expectedPreviousHandler = SIG_ERR;
	} else {
		*expectedPreviousHandler = SIG_IGN;
	}
#if !defined(OMR_OS_WINDOWS)
	primaryMasked = existingPrimary;
	secondaryMasked = false;
#endif /* !defined(OMR_OS_WINDOWS) */
	return OMR_ERROR_NONE;
}

static bool
checkPreviousHandler(int signum, TestAction testFunc, sighandler_t expectedPreviousHandler, sighandler_t previousHandler)
{
	/* Check returned previous handler to expected value. */
	if (signum == signumOptions[1]) {
		if ((testFunc == test_signal) || (testFunc == test_omrsig_primary_signal)) {
			expectedPreviousHandler = SIG_ERR;
		} else {
			expectedPreviousHandler = SIG_DFL;
		}
	}
	return previousHandler == expectedPreviousHandler;
}

static omr_error_t
performTestAndCheck(int expectedHandlerCalls, bool onStackCondition, bool testingSecondaryHandler, int signum)
{
	omr_error_t rc = OMR_ERROR_NONE;

#if !defined(OMR_OS_WINDOWS)
	if (signum != signumOptions[1]) {
#if !defined(J9ZOS390)
		/* Find the system implementation of sigaction on first call. */
		if (NULL == sigactionOS) {
			sigactionOS = (SIGACTION)dlsym(RTLD_NEXT, "sigaction");
		}
		if (NULL == sigactionOS) {
			void *handle = dlopen(NULL, RTLD_LAZY);
			sigactionOS = (SIGACTION)dlsym(handle, "sigaction");
		}
		if (NULL == sigactionOS) {
			rc = OMR_ERROR_INTERNAL;
			printf("dlsym() for sigaction failed at %s:%d.\n", __FILE__, __LINE__);
		}
#endif /* !defined(J9ZOS390) */

		if (OMR_ERROR_INTERNAL == rc) {
			struct sigaction actprimary = {{0}};
			struct sigaction actsecondary = {{0}};
			stack_t stack;
			sigactionOS(signum, NULL, &actprimary);
			sigaction(signum, NULL, &actsecondary);
			if (0 != sigaltstack(NULL, &stack)) {
				rc = OMR_ERROR_INTERNAL;
				printf("sigaltstack() failed in verifying test results at %s:%d.\n", __FILE__, __LINE__);
			}

			/* Check that the installed handlers have the correct alternate stack flags. */
			bool currentlyOnstack = (actprimary.sa_flags & SA_ONSTACK) && handlerIsFunction(&actprimary) && (SS_DISABLE != stack.ss_flags);
			bool secondaryOnstack = ((actsecondary.sa_flags & SA_ONSTACK) && handlerIsFunction(&actsecondary)) == testingSecondaryHandler;
			if ((onStackCondition != currentlyOnstack) && handlerIsFunction(&actprimary) && (actprimary.sa_handler != actsecondary.sa_handler)) {
				rc = OMR_ERROR_INTERNAL;
				printf("Signal alternate stack not correctly set: primary handler %scurrently on stack while %sin on stack condition at %s:%d.\n",
					   currentlyOnstack ? "" : "not ",
					   onStackCondition ? "" : "not ",
					   __FILE__, __LINE__);
			} else if (onStackCondition && !secondaryOnstack) {
				rc = OMR_ERROR_INTERNAL;
				printf("Signal alternate stack not correctly set: secondary handler not on alternate stack while in on stack condition at %s:%d.\n",
					   __FILE__, __LINE__);
			}
		}
	}
	sigset_t mask;
	sigemptyset(&mask);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);
#endif /* !defined(OMR_OS_WINDOWS) */

	/* Perform the signal test and check the expected number of calls. */
	for (int i = 0; i < 2; i += 1) {
		if ((65 > signum) && ((expectedHandlerCalls - handlerCalls > 0) || (SIGABRT != signum))) {
			if (SIGABRT == signum) {
				int newHandlerCalls = sigsetjmp(env, 1);
				if (0 == newHandlerCalls) {
					raise(signum);
				}
				handlerCalls = newHandlerCalls;
			} else {
				raise(signum);
			}
		}
	}

	if (expectedHandlerCalls != handlerCalls) {
		rc = OMR_ERROR_INTERNAL;
		printf("Expected %d handler calls, %d properly invoked. Failed at %s:%d.\n",
			   expectedHandlerCalls, handlerCalls, __FILE__, __LINE__);
	}

	return rc;
}

#if !defined(OMR_OS_WINDOWS)
static void
checkSignalMask(int signum, bool expected, bool checkingPrimary)
{
	sigset_t currentMask;
	pthread_sigmask(SIG_SETMASK, NULL, &currentMask);
	if (sigismember(&currentMask, signum) != expected) {
		handlerCalls -= 1;
		printf("Incorrect signal mask in %s signal handler. Expected signal to %sbe masked at %s:%d.\n",
			   checkingPrimary ? "primary" : "secondary",
			   expected ? "" : "not ",
			   __FILE__, __LINE__);
	}
}
#endif /* !defined(OMR_OS_WINDOWS) */

static void
handlerPrimary(int sig)
{
	handlerCalls += 1;
#if !defined(OMR_OS_WINDOWS)
	checkSignalMask(sig, primaryMasked, true);
#endif /* !defined(OMR_OS_WINDOWS) */
	if ((OMRSIG_RC_SIGNAL_HANDLED != omrsig_handler(sig, NULL, NULL)) && (SIGABRT == sig)) {
		siglongjmp(env, handlerCalls);
	}
}

static void
handlerSecondary(int sig)
{
	handlerCalls += 1;
#if !defined(OMR_OS_WINDOWS)
	checkSignalMask(sig, secondaryMasked, false);
#endif /* !defined(OMR_OS_WINDOWS) */
	if (SIGABRT == sig) {
		siglongjmp(env, handlerCalls);
	}
}

#if !defined(OMR_OS_WINDOWS)
static void
handlerPrimaryInfo(int sig, siginfo_t *siginfo, void *uc)
{
	handlerCalls += 1;
	checkSignalMask(sig, primaryMasked, true);
	if ((OMRSIG_RC_SIGNAL_HANDLED != omrsig_handler(sig, NULL, NULL)) && (SIGABRT == sig)) {
		siglongjmp(env, handlerCalls);
	}
}

static void
handlerSecondaryInfo(int sig, siginfo_t *siginfo, void *uc)
{
	handlerCalls += 1;
	checkSignalMask(sig, secondaryMasked, false);
	if (SIGABRT == sig) {
		siglongjmp(env, handlerCalls);
	}
}
#endif /* !defined(OMR_OS_WINDOWS) */

static void
handlerPrimaryInstaller(int sig)
{
	handlerCalls += 1;
	if ((OMRSIG_RC_SIGNAL_HANDLED != omrsig_handler(sig, NULL, NULL))) {
		siglongjmp(env, handlerCalls);
	}
}

static void
handlerSecondaryInstaller(int sig)
{
	handlerCalls += 1;
	signal(sig, handlerTertiaryInstaller);
	raise(sig);
	siglongjmp(env, handlerCalls);
}

static void
handlerTertiaryInstaller(int sig)
{
	handlerCalls += 1;
	siglongjmp(env, handlerCalls);
}
