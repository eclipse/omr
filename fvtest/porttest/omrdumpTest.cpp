/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2016
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
 * @brief Verify port library core file creation.
 *
 * Exercise the API for port library virtual memory management operations.  These functions
 * can be found in the file @ref omrosdump.c
 *
 *
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(AIXPPC)
#include <sys/types.h>
#include <sys/var.h>
#endif

#include <signal.h>

#if defined(WIN32) || defined(WIN64)
/* for getcwd() */
#include <direct.h>
#define getcwd _getcwd
#endif

#include "testHelpers.hpp"
#include "omrport.h"

#define allocNameSize 64 /**< @internal buffer size for name function */

static uintptr_t simpleHandlerFunction(struct OMRPortLibrary *portLibrary, uint32_t gpType, void *gpInfo, void *handler_arg);
static void removeDump(OMRPortLibrary *portLib, const char *filename, const char *testName);

typedef struct simpleHandlerInfo {
	char *coreFileName;
	const char *testName;
} simpleHandlerInfo;

static void
removeDump(OMRPortLibrary *portLib, const char *filename, const char *testName)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	bool removeDumpSucceeded = true;

	/* Delete the file if possible. */
#if defined(J9ZOS390)
	char deleteCore[EsMaxPath] = {0};
	sprintf(deleteCore, "tso delete %s", (strstr(filename, ".") + 1));

	char *ending = strstr(deleteCore, ".X&DS");
	if (NULL != ending) {
		strncpy(ending, ".X001", 5);
	}
	if (-1 == system(deleteCore)) {
		removeDumpSucceeded = false;
	}
#else /* defined(J9ZOS390) */
	if (0 != remove(filename)) {
		removeDumpSucceeded = false;
	}
#endif /* defined(J9ZOS390) */
	if (removeDumpSucceeded) {
		outputComment(OMRPORTLIB, "\tremoved: %s\n", filename);
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\tfailed to remove %s\n", filename);
	}
}

/**
 * Verify port library memory management.
 *
 * Ensure the port library is properly setup to run core file generation operations.
 */
TEST(PortDumpTest, dump_verify_functiontable_slots)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrdump_verify_functiontable_slots";

	reportTestEntry(OMRPORTLIB, testName);


	/* omrmem_test2 */
	if (NULL == OMRPORTLIB->dump_create) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->vmem_reserve_memory is NULL\n");
	}
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Call omrdump_create() without passing in core file name. This does not actually test that a core file was actually created.
 */
TEST(PortDumpTest, dump_test_create_dump_with_NO_name)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrdump_test_create_dump_with_NO_name";
	uintptr_t rc = 99;
	char coreFileName[EsMaxPath];
	BOOLEAN doFileVerification = FALSE;
#if defined(AIXPPC)
	struct vario myvar;
	int sys_parmRC;
#endif

	reportTestEntry(OMRPORTLIB, testName);

	coreFileName[0] = '\0';

#if 0
	/* try out a NULL test (turns out this crashes) */
	rc = omrdump_create(NULL, NULL, NULL); /* this crashes */
#endif

	/* try out a more sane NULL test */
	outputComment(OMRPORTLIB, "calling omrdump_create with empty filename\n");

#if defined(J9ZOS390)
	rc = omrdump_create(coreFileName, "IEATDUMP", NULL);
#else
	rc = omrdump_create(coreFileName, NULL, NULL);
#endif

	if (rc == 0) {
		uintptr_t verifyFileRC = 99;

		outputComment(OMRPORTLIB, "omrdump_create claims to have written a core file to: %s\n", coreFileName);


#if defined(AIXPPC)
		/* We defer to fork abort on AIX machines that don't have "Enable full CORE dump" enabled in smit,
		 * in which case omrdump_create will not return the filename.
		 * So, only check for a specific filename if we are getting full core dumps */
		sys_parmRC = sys_parm(SYSP_GET, SYSP_V_FULLCORE, &myvar);

		if ((sys_parmRC == 0) && (myvar.v.v_fullcore.value == 1)) {

			doFileVerification = TRUE;
		}
#else /* defined(AIXPPC) */
		doFileVerification = TRUE;
#endif /* defined(AIXPPC) */
		if (doFileVerification) {
			verifyFileRC = verifyFileExists(PORTTEST_ERROR_ARGS, coreFileName);
			if (verifyFileRC == 0) {
				removeDump(OMRPORTLIB, coreFileName, testName);
			}
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrdump_create returned: %u, with filename: %s", rc, coreFileName);
	}
	reportTestExit(OMRPORTLIB, testName);
}


/**
 * Test omrdump_create(), this time passing in core file name
 * Verify that the returned core file name actually exists.
 *
 * @note this only tests IEATDUMP generation on z/OS
 */
TEST(PortDumpTest, dump_test_create_dump_with_name)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrdump_test_create_dump_with_name";
	uintptr_t rc = 99;
	char buff[EsMaxPath];
	char *coreFileName = NULL;

	reportTestEntry(OMRPORTLIB, testName);

#if defined(J9ZOS390)
	coreFileName = atoe_getcwd(buff, EsMaxPath);
#else
	coreFileName = getcwd(buff, EsMaxPath);
#endif

	strncat(coreFileName, "/", EsMaxPath);	/* make sure the directory ends with a slash */
	strncat(coreFileName, testName, EsMaxPath);

	outputComment(OMRPORTLIB, "calling omrdump_create with filename: %s\n", coreFileName);
#if !defined(J9ZOS390)
	rc = omrdump_create(coreFileName, NULL, NULL);
#else
	rc = omrdump_create(coreFileName, "IEATDUMP", NULL);
#endif

	if (rc == 0) {
		uintptr_t verifyFileRC = 99;

		outputComment(OMRPORTLIB, "omrdump_create claims to have written a core file to: %s\n", coreFileName);
		verifyFileRC = verifyFileExists(PORTTEST_ERROR_ARGS, coreFileName);
		if (verifyFileRC == 0) {
			removeDump(OMRPORTLIB, coreFileName, testName);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrdump_create returned: %u, with filename: %s\n", rc, coreFileName);
	}
	reportTestExit(OMRPORTLIB, testName);
}



/**
 * Test calling omrdump_create() from a signal handler.
 *
 * Verify that the returned core file name actually exists.
 */
TEST(PortDumpTest, dump_test_create_dump_from_signal_handler)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrdump_test_create_dump_from_signal_handler";
	char buff[EsMaxPath];
	char *coreFileName = NULL;
	uintptr_t result;
	uint32_t protectResult;
	simpleHandlerInfo handlerInfo;
	uint32_t sig_protectFlags;

#if defined(J9ZOS390)
	coreFileName = atoe_getcwd(buff, EsMaxPath);
#else
	coreFileName = getcwd(buff, EsMaxPath);
#endif

	strncat(coreFileName, "/", EsMaxPath);	/* make sure the directory ends with a slash */
	strncat(coreFileName, testName, EsMaxPath);

	sig_protectFlags = OMRPORT_SIG_FLAG_SIGSEGV | OMRPORT_SIG_FLAG_MAY_RETURN;

	reportTestEntry(OMRPORTLIB, testName);

	if (!omrsig_can_protect(sig_protectFlags)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect does not offer adequate protection for test\n");
	} else {
		handlerInfo.coreFileName = coreFileName;
		handlerInfo.testName = testName;
		protectResult = omrsig_protect(
							raiseSEGV, NULL,
							simpleHandlerFunction, &handlerInfo,
							sig_protectFlags,
							&result);

		if (protectResult != OMRPORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 return value\n");
		}
	}
	reportTestExit(OMRPORTLIB, testName);
}


static uintptr_t
simpleHandlerFunction(struct OMRPortLibrary *portLibrary, uint32_t gpType, void *gpInfo, void *handler_arg)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	simpleHandlerInfo *info = (simpleHandlerInfo *)handler_arg;
	const char *testName = info->testName;
	uintptr_t rc;

	outputComment(OMRPORTLIB, "calling omrdump_create with filename: %s\n", info->coreFileName);

#if defined(J9ZOS390)
	rc = omrdump_create(info->coreFileName, "IEATDUMP", NULL);
#else
	rc = omrdump_create(info->coreFileName, NULL, NULL);
#endif

	if (rc == 0) {
		uintptr_t verifyFileRC = 99;

		outputComment(OMRPORTLIB, "omrdump_create claims to have written a core file to: %s\n", info->coreFileName);
		verifyFileRC = verifyFileExists(PORTTEST_ERROR_ARGS, info->coreFileName);
		if (verifyFileRC == 0) {
			removeDump(OMRPORTLIB, info->coreFileName, testName);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrdump_create returned: %u, with filename: %s", rc, info->coreFileName);
	}

	return OMRPORT_SIG_EXCEPTION_RETURN;
}
