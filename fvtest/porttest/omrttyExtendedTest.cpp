/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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


/**
 * @file
 * @ingroup PortTest
 * @brief Verify TTY operations
 *
 * Exercise the omrtty_daemonize API for port library functions in @ref omrtty.c
 *
 *
 */
#include <string.h>
#include <stdio.h>
#if defined(WIN32)
#include <windows.h>
#endif /* defined(WIN32) */

#include "testHelpers.hpp"
#include "omrport.h"

/**
 * Verify that a call to omrtty_printf/omrtty_vprintf after a call to omrtty_daemonize does not crash.
 *
 * Call omrtty_daemonize, then call omrtty_printf/omrtty_vprintf. If we crash or print anything, then the test has failed.
 */
#if defined(WIN32)
TEST(PortTtyTest, tty_daemonize_test)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtty_daemonize_test";
	const char *format = "Test string %d %d %d\n";

	reportTestEntry(OMRPORTLIB, testName);

	/* Get current output handles. */
	HANDLE stdoutOriginal = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE stderrOriginal = GetStdHandle(STD_ERROR_HANDLE);

	/* Redirect output to a test handle and restart TTY to get the port functions to use it. */
	HANDLE stdoutWrite;
	HANDLE stdoutRead;
	CreatePipe(&stdoutRead, &stdoutWrite, NULL, 0);
	SetStdHandle(STD_OUTPUT_HANDLE, stdoutWrite);
	SetStdHandle(STD_ERROR_HANDLE, stdoutWrite);
	omrtty_shutdown();
	omrtty_startup();

	omrtty_printf("pre-daemonize\n");
	omrtty_daemonize();
	/* outpuComment uses omrtty_vprintf */
	outputComment(OMRPORTLIB, format, 1, 2, 3);
	omrtty_printf("post-daemonize\n");

	char readBuffer[EsMaxPath] = {0};
	DWORD bytesRead;
	ReadFile(stdoutRead, readBuffer, EsMaxPath - 1, &bytesRead, NULL);

	/* Restart TTY to resume output for remaining tests. */
	SetStdHandle(STD_OUTPUT_HANDLE, stdoutOriginal);
	SetStdHandle(STD_ERROR_HANDLE, stderrOriginal);
	omrtty_shutdown();
	omrtty_startup();

	/* Check the output. */
	outputComment(OMRPORTLIB, "Captured test output: %s\n", readBuffer);
	if (NULL == strstr(readBuffer, "post-daemonize")) {
		outputComment(OMRPORTLIB, "Test passed: 'post-daemonize' not found in test output.\n");
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Test failed: post-daemonize unexpectedly found in output.\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}
#endif /* defined(WIN32) */
