/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "testHelpers.hpp"
#include "omrport.h"

extern PortTestEnvironment *portTestEnv;

typedef struct PortTestStruct {
	char *fileName;
	char *testName;
	char *errorMessage;
	char *portErrorMessage;
	int32_t portErrorNumber;
	int32_t lineNumber;
} PortTestStruct;

#define MAX_NUM_TEST_FAILURES 256

/*
 * A log of all the test failures. logTestFailure() writes to this.
 *
 * A linked list would be better */
static PortTestStruct testFailures[MAX_NUM_TEST_FAILURES];

/*
 * Total number of failed test cases in all components
 *
 * logTestFailure increments this (note that C initializes this to 0)
 */
static int numTestFailures;

/**
 * Number of failed tests
 *
 * When @ref reportTestEntry is executed this global variable is cleared.  When
 * an error message is output via @ref outputMessage it is incremented.  Upon exit
 * of a testcase @ref reportTestExit will indicate if the test passed or failed based
 * on this variable.
 */
static int numberFailedTestsInComponent;

/**
 * Display a message indicating the operation is not supported
 *
 * The port library have various configurations to save size on very small VMs.
 * As a result entire areas of functionality may not be present.  Sockets and file systems
 * are examples of these areas.  In addition even if an area is present not all functionality
 * for that area may be available.  Again sockets and file systems are examples.
 *
 * @param[in] portLibrary The port library
 * @param[in] operation The operation that is not supported
 */
void
operationNotSupported(struct OMRPortLibrary *portLibrary, const char *operationName)
{
	portTestEnv->log("%s not supported in this configuration\n", operationName);
}

/**
 * Display a message indicating the test is starting
 *
 * @param[in] portLibrary The port library
 * @param[in] testName The test that is starting
 *
 * @note Clears number of failed tests.  @see outputMessage.
 */
void
reportTestEntry(struct OMRPortLibrary *portLibrary, const char *testName)
{
	portTestEnv->log("\nStarting test %s\n", testName);
	numberFailedTestsInComponent = 0;
}

/**
 * Display a message indicating the test is finished
 *
 * @param[in] portLibrary The port library
 * @param[in] testName The test that finished
 *
 * @return TEST_PASS if no tests failed, else TEST_FAILED as reported
 * by @ref outputMessage
 */
int
reportTestExit(struct OMRPortLibrary *portLibrary, const char *testName)
{
	portTestEnv->log("Ending test %s\n", testName);
	EXPECT_TRUE(0 == numberFailedTestsInComponent) << "Test failed!";
	return 0 == numberFailedTestsInComponent ? TEST_PASS : TEST_FAIL;
}

/**
 * Iterate through the global @ref testFailures printing out each failed test case
 *
 * The memory for each string in testFailers is freed after writing the string to the console.
 *
 * @param[in] 	portLibrary
 * @param[out] 	dest container for pointer to copied memory
 * @param[in]	source string to be copied into dest
 */
void
dumpTestFailuresToConsole(struct OMRPortLibrary *portLibrary)
{
	int32_t i;

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	portTestEnv->log(LEVEL_ERROR, "-------------------------------------------------------------------------\n");
	portTestEnv->log(LEVEL_ERROR, "-------------------------------------------------------------------------\n\n");
	portTestEnv->log(LEVEL_ERROR, "FAILURES DETECTED. Number of failed tests: %u\n\n", numTestFailures);

	for (i = 0; i < numTestFailures ; i++) {
		portTestEnv->log(LEVEL_ERROR, "%i: %s\n", i + 1, testFailures[i].testName);
		portTestEnv->log(LEVEL_ERROR, "\t%s line %4zi: %s\n", testFailures[i].fileName, testFailures[i].lineNumber, testFailures[i].errorMessage);
		portTestEnv->log(LEVEL_ERROR, "\t\tLastErrorNumber: %i\n", testFailures[i].portErrorNumber);
		portTestEnv->log(LEVEL_ERROR, "\t\tLastErrorMessage: %s\n\n", testFailures[i].portErrorMessage);

		omrmem_free_memory(testFailures[i].fileName);
		omrmem_free_memory(testFailures[i].testName);
		omrmem_free_memory(testFailures[i].errorMessage);
		omrmem_free_memory(testFailures[i].portErrorMessage);
	}

}

/**
 * Allocate memory for destination then copy the source into it.
 *
 * @param[in] 	portLibrary
 * @param[out] 	dest container for pointer to copied memory
 * @param[in]	source string to be copied into dest
 */
static void
allocateMemoryForAndCopyInto(struct OMRPortLibrary *portLibrary, char **dest, const char *source)
{

	uintptr_t strLenPlusTerminator = 0;

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	strLenPlusTerminator = strlen(source) + 1 /*null terminator */;
	*dest = (char *)omrmem_allocate_memory(strLenPlusTerminator, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (*dest == NULL)  {
		return;
	} else {
		strncpy(*dest, source, strLenPlusTerminator);
	}
}
/**
 * Log the test failure so that it can be dumped at test harness exit.
 *
 * Allocate the memory required to log the failures. This memory is freed in @ref dumpTestFailuresToConsole
 *
 * @param[in] portLibrary
 * @param[in] testName null-terminated string;
 * @param[in] errorMessage null-terminated string;
 * @param[in] lineNumber;
 *
 * This uses the global numTestFailures */
static void
logTestFailure(struct OMRPortLibrary *portLibrary, const char *fileName, int32_t lineNumber, const char *testName, int32_t portErrorNumber, const char *portErrorMessage, const char *testErrorMessage)
{

	if (MAX_NUM_TEST_FAILURES <= numTestFailures) {
		return;
	}

	allocateMemoryForAndCopyInto(portLibrary, &testFailures[numTestFailures].testName, testName);
	allocateMemoryForAndCopyInto(portLibrary, &testFailures[numTestFailures].errorMessage, testErrorMessage);
	allocateMemoryForAndCopyInto(portLibrary, &testFailures[numTestFailures].fileName, fileName);
	allocateMemoryForAndCopyInto(portLibrary, &testFailures[numTestFailures].portErrorMessage, portErrorMessage);

	testFailures[numTestFailures].lineNumber = lineNumber;
	testFailures[numTestFailures].portErrorNumber = portErrorNumber;

	numTestFailures++;
}

/**
 * Write a formatted string to the console, lookup the last port error message and port error number, then store it.
 *
 * Update the number of failed tests
 *
 *
 * @param[in] portLibrary The port library
 * @param[in] fileName File requesting message output
 * @param[in] lineNumber Line number in the file of request
 * @param[in] testName Name of the test requesting output
 * @param[in] foramt Format of string to be output
 * @param[in] ... argument list for format string
 */
void
outputErrorMessage(struct OMRPortLibrary *portLibrary, const char *fileName, int32_t lineNumber, const char *testName, const char *format, ...)
{

	char *buf, *portErrorBuf = NULL;
	uintptr_t sizeBuf;
	size_t sizePortErrorBuf;
	va_list args;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	lastErrorMessage = (char *)omrerror_last_error_message();
	lastErrorNumber = omrerror_last_error_number();

	/* Capture the error message now
	 * Get the size needed to hold the last error message (don't use str_printf to avoid polluting error message */
	sizePortErrorBuf = strlen(lastErrorMessage) + 1 /* for the null terminator */;

	portErrorBuf = (char *)omrmem_allocate_memory(sizePortErrorBuf, OMRMEM_CATEGORY_PORT_LIBRARY);

	if (NULL != portErrorBuf) {
		strncpy(portErrorBuf, lastErrorMessage, sizePortErrorBuf);
	} else {
		portTestEnv->log(LEVEL_ERROR, "\n\n******* omrmem_allocate_memory failed to allocate %i bytes, exiting.\n\n", sizePortErrorBuf);
		exit(EXIT_OUT_OF_MEMORY);
	}

	va_start(args, format);
	/* get the size needed to hold the error message that was passed in */
	sizeBuf = omrstr_vprintf(NULL, 0, format, args);

	buf = (char *)omrmem_allocate_memory(sizeBuf, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL != buf) {
		omrstr_vprintf(buf, sizeBuf, format, args);
	} else {
		portTestEnv->log(LEVEL_ERROR, "\n\n******* omrmem_allocate_memory failed to allocate %i bytes, exiting.\n\n", sizeBuf);
		exit(EXIT_OUT_OF_MEMORY);

	}
	va_end(args);

	portTestEnv->log(LEVEL_ERROR, "%s line %4zi: %s ", fileName, lineNumber, testName);
	portTestEnv->log(LEVEL_ERROR, "%s\n", buf);
	portTestEnv->log(LEVEL_ERROR, "\t\tLastErrorNumber: %i\n", lastErrorNumber);
	portTestEnv->log(LEVEL_ERROR, "\t\tLastErrorMessage: %s\n\n", portErrorBuf);

	logTestFailure(OMRPORTLIB, fileName, lineNumber, testName, lastErrorNumber, portErrorBuf, buf);

	omrmem_free_memory(portErrorBuf);
	omrmem_free_memory(buf);

	numberFailedTestsInComponent++;
}

/**
 * Display an eyecatcher prior to starting a test suite.
 */
void
HEADING(struct OMRPortLibrary *portLibrary, const char *string)
{
	const char *dash = "----------------------------------------";
	portTestEnv->log("%s\n%s\n%s\n\n", dash, string, dash);
}

/**
 * Check that the file exists on the file system.
 *
 * This takes the porttest filename and line number information so that the caller is properly identified as failing.
 *
 * @param[in] portLibrary The port library
 * @param[in] fileName File requesting file check
 * @param[in] lineNumber Line number in the file of request
 * @param[in] testName Name of the test requesting file check
 * @param[in] fileName file who's existence we need to verify
 * @param[in] lineNumber Line number in the file of request
 *
 * @return 0 if the file exists, non-zero otherwise.
 */
uintptr_t
verifyFileExists(struct OMRPortLibrary *portLibrary, const char *pltestFileName, int32_t lineNumber, const char *testName, const char *fileName)
{
	uintptr_t rc = 1;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	portTestEnv->changeIndent(1);

#if defined(J9ZOS390)
	char dumpName[EsMaxPath] = {0};

	/* Replace trailing ".X&DS" on 64bit and add required "//" prefix */
	strncpy(dumpName, "//", 2);
	strncat(dumpName, (strstr(fileName, ".") + 1), strlen(fileName));
	char *ending = strstr(dumpName, ".X&DS");
	if (NULL != ending) {
		strncpy(ending, ".X001", 5);
	}

	portTestEnv->log("checking for data set: %s\n", dumpName);
	FILE *file = fopen(dumpName, "r");
	if (NULL == file) {
		outputErrorMessage(OMRPORTLIB, pltestFileName, lineNumber, testName, "\tdata set: %s does not exist!\n", -1, fileName);
	} else {
		portTestEnv->log("data set: %s exists\n", dumpName);
		fclose(file);
		rc = 0;
	}
#else /* defined(J9ZOS390) */
	J9FileStat fileStat;
	int32_t fileStatRC = 99;

	/* stat the fileName */
	fileStatRC = omrfile_stat(fileName, 0, &fileStat);

	if (0 == fileStatRC) {
		if (fileStat.isFile) {
			portTestEnv->log("file: %s exists\n", fileName);
			rc = 0;
		} else {
			outputErrorMessage(OMRPORTLIB, pltestFileName, lineNumber, testName, "\tfile: %s does not exist!\n", -1, fileName);
		}
	} else {
		/* error in file_stat */
		outputErrorMessage(OMRPORTLIB, pltestFileName, lineNumber, testName, "\nomrfile_stat call in verifyFileExists() returned %i: %s\n", fileStatRC, omrerror_last_error_message());
	}
#endif /* defined(J9ZOS390) */

	portTestEnv->changeIndent(-1);
	return rc;
}


/**
 * Removes a directory by recursively removing sub-directory and files.
 *
 * @param[in] portLibrary The port library
 * @param[in] directory to clean up
 *
 * @return void
 */
void
deleteControlDirectory(struct OMRPortLibrary *portLibrary, char *baseDir)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	struct J9FileStat buf;

	omrfile_stat(baseDir, (uint32_t)0, &buf);

	if (buf.isDir != 1) {
		omrfile_unlink(baseDir);
	} else {
		char mybaseFilePath[EsMaxPath];
		char resultBuffer[EsMaxPath];
		uintptr_t rc, handle;

		omrstr_printf(mybaseFilePath, EsMaxPath, "%s", baseDir);
		rc = handle = omrfile_findfirst(mybaseFilePath, resultBuffer);
		while ((uintptr_t)-1 != rc) {
			char nextEntry[EsMaxPath];
			/* skip current and parent dir */
			if (resultBuffer[0] == '.') {
				rc = omrfile_findnext(handle, resultBuffer);
				continue;
			}
			omrstr_printf(nextEntry, EsMaxPath, "%s/%s", mybaseFilePath, resultBuffer);
			deleteControlDirectory(OMRPORTLIB, nextEntry);
			rc = omrfile_findnext(handle, resultBuffer);
		}
		if (handle != (uintptr_t)-1) {
			omrfile_findclose(handle);
		}
		omrfile_unlinkdir(mybaseFilePath);
	}
}

/**
 * Memory category walk function used by getPortLibraryMemoryCategoryData
 */
static uintptr_t
categoryWalkFunction(uint32_t categoryCode, const char *categoryName, uintptr_t liveBytes, uintptr_t liveAllocations, BOOLEAN isRoot, uint32_t parentCategoryCode, OMRMemCategoryWalkState *walkState)
{
	uintptr_t *blocks = (uintptr_t *)walkState->userData1;
	uintptr_t *bytes = (uintptr_t *)walkState->userData2;

	if (OMRMEM_CATEGORY_PORT_LIBRARY == categoryCode) {
		*blocks = liveAllocations;
		*bytes = liveBytes;

		return J9MEM_CATEGORIES_STOP_ITERATING;
	} else {
		return J9MEM_CATEGORIES_KEEP_ITERATING;
	}
}

/**
 * Walks the port library memory categories and extracts the
 * values for OMRMEM_CATEGORY_PORT_LIBRARY
 *
 * @param[in]          portLibrary    Port library
 * @param[out]         blocks         Pointer to memory to store the live blocks for the port library category
 * @param[out]         bytes          Pointer to memory to store the live bytes for the port library category
 */
void
getPortLibraryMemoryCategoryData(struct OMRPortLibrary *portLibrary, uintptr_t *blocks, uintptr_t *bytes)
{
	OMRMemCategoryWalkState walkState;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	memset(&walkState, 0, sizeof(OMRMemCategoryWalkState));

	walkState.userData1 = (void *)blocks;
	walkState.userData2 = (void *)bytes;
	walkState.walkFunction = &categoryWalkFunction;

	omrmem_walk_categories(&walkState);
}

/* returns 1 if the @ref s starts with @ref prefix */
int
startsWith(const char *s, const char *prefix)
{
	int sLen = (int)strlen(s);
	int prefixLen = (int)strlen(prefix);
	int i;

	if (sLen < prefixLen) {
		return 0;
	}
	for (i = 0; i < prefixLen; i += 1) {
		if (s[i] != prefix[i]) {
			return 0;
		}
	}
	return 1; /*might want to make sure s is not NULL or something*/
}

uintptr_t
raiseSEGV(OMRPortLibrary *portLibrary, void *arg)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char *testName = (const char *)arg;

#if defined(WIN32) || defined (J9ZOS390)
	/*
	 * - Windows structured exception handling doesn't interact with raise().
	 * - z/OS doesn't provide a value for the psw1 (PC) register when you use raise()
	 */
	char *ptr = (char *)(-1);
	ptr -= 4;
	*ptr = -1;
#else
	raise(SIGSEGV);
#endif

	outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpectedly continued execution");

	return 8096;
}

/* Clean up the test output when test case passes */
void
testFileCleanUp(const char* filePrefix)
{
	if (::testing::UnitTest::GetInstance()->current_test_case()->Passed()) {
		OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
		uintptr_t rc = -1;
		uintptr_t handle = -1;
		char resultBuffer[1024];
		rc = handle = omrfile_findfirst("./", resultBuffer);
		while ((uintptr_t)-1 != rc) {
			if (0 == strncmp(resultBuffer, filePrefix, strlen(filePrefix))) {
				omrfile_unlink(resultBuffer);
			}
			rc = omrfile_findnext(handle, resultBuffer);
		}
		if ((uintptr_t)-1 != handle) {
			omrfile_findclose(handle);
		}
	}
}
