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


/*
 * $RCSfile: omrmmapTest.c,v $
 * $Revision: 1.38 $
 * $Date: 2011-12-30 14:15:00 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library file system.
 *
 * Exercise the API for port library mmap system operations.  These functions
 * can be found in the file @ref omrmmap.c
 *
 * @note port library mmap system operations are not optional in the port library table.
 * @note The default processing if memory mapping is not supported by the OS is to allocate
 *       memory and read in the file.  This is the equivalent of copy-on-write mmapping
 *
 */
#include <stdlib.h>
#include <string.h>

#include "testHelpers.hpp"
#include "testProcessHelpers.hpp"
#include "omrfileTest.h"
#include "omrport.h"

#define J9MMAP_PROTECT_TESTS_BUFFER_SIZE 100

typedef struct simpleHandlerInfo {
	const char *testName;
} simpleHandlerInfo;

typedef struct protectedWriteInfo {
	const char *testName;
	const char *text;
	int shouldCrash;
	char *address;
} protectedWriteInfo;

static uintptr_t
simpleHandlerFunction(struct OMRPortLibrary *portLibrary, uint32_t gpType, void *gpInfo, void *handler_arg)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	outputComment(OMRPORTLIB, " A crash occured, signal handler invoked (type = 0x%x)\n", gpType);
	return OMRPORT_SIG_EXCEPTION_RETURN;
}


static uintptr_t
protectedWriter(OMRPortLibrary *portLibrary, void *arg)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	struct protectedWriteInfo *info = (protectedWriteInfo *)arg;
	const char *testName = info->testName;

	/* verify that we can write to the file */
	strcpy(info->address, info->text);

	if (info->shouldCrash) {
		/* we should have crashed in the above call, so if we got here we didn't have protection */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unexpectedly continued execution");
	}
	return ~OMRPORT_SIG_EXCEPTION_OCCURRED;
}

/**
 * Verify port library properly setup to run mmap tests
 */
TEST(PortMmapTest, mmap_test0)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test0";

	reportTestEntry(OMRPORTLIB, testName);

	/* Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORTLIB->mmap_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_startup is NULL\n");
	}

	/* Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORTLIB->mmap_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_shutdown is NULL\n");
	}

	if (NULL == OMRPORTLIB->mmap_map_file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_map_file is NULL\n");
	}

	if (NULL == OMRPORTLIB->mmap_unmap_file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_unmap_file is NULL\n");
	}

	if (NULL == OMRPORTLIB->mmap_msync) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_msync is NULL\n");
	}

	if (NULL == OMRPORTLIB->mmap_protect) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_protect is NULL\n");
	}

	if (NULL == OMRPORTLIB->mmap_get_region_granularity) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->mmap_get_region_granularity is NULL\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}



/**
 * Verify port memory mapping.
 *
 * Verify @ref omrmmap.c::omrmmap_map_file "omrmmap_map_file()" correctly
 * processes read-only mapping
 */
TEST(PortMmapTest, mmap_test1)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test1";

	const char *filename = "mmapTest1.tst";
	intptr_t fd;
	intptr_t rc;

#define J9MMAP_TEST1_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST1_BUFFER_SIZE];
	int i = 0;
	const char *readableText = "Here is some readable text in the file";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	/* Initialize buffer to be written */
	for (i = 0; i < J9MMAP_TEST1_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);

	omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_write(fd, buffer, J9MMAP_TEST1_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(fd);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, NULL, OMRPORT_MMAP_FLAG_READ, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	/* For Windows CE we should really close th mapping handle after the unmap */
	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(mapAddr + 30, readableText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Strings do not match in mapped area\n");
	}

	omrmmap_unmap_file(mmapHandle);

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port memory mapping.
 *
 * Verify @ref omrmmap.c::omrmmap_map_file "omrmmap_map_file()" correctly
 * processes write mapping
 */
TEST(PortMmapTest, mmap_test2)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test2";

	const char *filename = "mmapTest2.tst";
	intptr_t fd;
	intptr_t rc;

#define J9MMAP_TEST2_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST2_BUFFER_SIZE];
	char updatedBuffer[J9MMAP_TEST2_BUFFER_SIZE];
	int i = 0;
	const char *readableText = "Here is some readable text in the file";
	const char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	int32_t mmapCapabilities = omrmmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	if (!(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_WRITE)) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_WRITE not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Initialize buffer to be written */
	for (i = 0; i < J9MMAP_TEST2_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);

	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_write(fd, buffer, J9MMAP_TEST2_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(fd);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, NULL, OMRPORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	strcpy(mapAddr + 30, modifiedText);

	omrmmap_unmap_file(mmapHandle);

	fd = omrfile_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_read(fd, updatedBuffer, J9MMAP_TEST2_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer + 30, modifiedText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port memory mapping.
 *
 * Verify @ref omrmmap.c::omrmmap_map_file "omrmmap_map_file()" correctly
 * processes copy-on-write mapping
 */
TEST(PortMmapTest, mmap_test3)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test3";

	const char *filename = "mmapTest3.tst";
	intptr_t fd;
	intptr_t rc;

#define J9MMAP_TEST3_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST3_BUFFER_SIZE];
	char updatedBuffer[J9MMAP_TEST3_BUFFER_SIZE];
	int i = 0;
	const char *readableText = "Here is some readable text in the file";
	const char *modifiedText = "This text is modified";
	int64_t fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	int32_t mmapCapabilities = omrmmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	if (!(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_COPYONWRITE)) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_COPYONWRITE not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Initialize buffer to be written */
	for (i = 0; i < J9MMAP_TEST3_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);

	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_write(fd, buffer, J9MMAP_TEST3_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(fd);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, NULL, OMRPORT_MMAP_FLAG_COPYONWRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	strcpy(mapAddr + 30, modifiedText);

	omrmmap_unmap_file(mmapHandle);

	fd = omrfile_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_read(fd, updatedBuffer, J9MMAP_TEST3_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer + 30, readableText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Original string does not match in updated buffer\n");
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port memory mapping.
 *
 * Verify @ref omrmmap.c::omrmmap_map_file "omrmmap_map_file()" correctly
 * processes msync updates of the file on disk
 */
TEST(PortMmapTest, mmap_test4)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test4";

	const char *filename = "mmapTest4.tst";
	intptr_t fd;
	intptr_t rc;

#define J9MMAP_TEST4_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST4_BUFFER_SIZE];
	char updatedBuffer[J9MMAP_TEST4_BUFFER_SIZE];
	int i = 0;
	const char *readableText = "Here is some readable text in the file";
	const char *modifiedText = "This text is modified";
	int64_t fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	int32_t mmapCapabilities = omrmmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	if (!(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_WRITE) || !(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_MSYNC)) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_WRITE or _MSYNC not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Initialize buffer to be written */
	for (i = 0; i < J9MMAP_TEST4_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);

	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_write(fd, buffer, J9MMAP_TEST4_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(fd);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead | EsOpenWrite | EsOpenSync, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, NULL, OMRPORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	strcpy(mapAddr + 30, modifiedText);

	rc = omrmmap_msync(mapAddr, (uintptr_t)fileLength, OMRPORT_MMAP_SYNC_WAIT);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Msync failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
	}

	fd = omrfile_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_read(fd, updatedBuffer, J9MMAP_TEST4_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer + 30, modifiedText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

	omrmmap_unmap_file(mmapHandle);

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port memory mapping.
 *
 * Verify @ref omrmmap.c::omrmmap_map_file "omrmmap_map_file()" correctly
 * processes the mappingName parameter on Windows and checks mapping
 * between processes
 *
 * Note: This test uses helper functions from omrfileTest.c
 */
TEST(PortMmapTest, mmap_test5)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRProcessHandle pid1 = 0;
	OMRProcessHandle pid2 = 0;
	intptr_t termstat1;
	intptr_t termstat2;
	int32_t mmapCapabilities = omrmmap_capabilities();
	const char *testName = "omrmmap_test5";
	char *argv0 = portTestEnv->_argv[0];

	reportTestEntry(OMRPORTLIB, testName);

	if (!(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_WRITE) || !(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_MSYNC)) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_WRITE or _MSYNC not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Remove status files */
	omrfile_unlink("omrmmap_test5_child_1_mapped_file");
	omrfile_unlink("omrmmap_test5_child_1_checked_file");
	omrfile_unlink("omrmmap_test5_child_2_changed_file");

	/* parent */
	pid1 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrmmap_test5_1");
	if (NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrmmap_test5_2");
	if (NULL == pid2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid2!\n");
		goto exit;
	}

	termstat1 = waitForTestProcess(OMRPORTLIB, pid1);
	termstat2 = waitForTestProcess(OMRPORTLIB, pid2);

	if (termstat1 != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Child process 1 exited with status %i: Expected 0.\n", termstat1);
	}
	if (termstat2 != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Child process 2 exited with status %i: Expected 0.\n", termstat2);
	}

exit:

	reportTestExit(OMRPORTLIB, testName);
}

int
omrmmap_test5_child_1(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char *testName = "omrmmap_test5_child_1";

	const char *filename = "mmapTest5.tst";
	const char *mappingName = "C:\\full\\path\\to\\mmapTest5.tst";
	intptr_t fd;
	intptr_t rc;

#define J9MMAP_TEST5_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST5_BUFFER_SIZE];
	int i = 0;
	const char *readableText = "Here is some readable text in the file";
	const char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	/* Initialize buffer to be written */
	for (i = 0; i < J9MMAP_TEST4_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);

	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_write(fd, buffer, J9MMAP_TEST4_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(fd);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	outputComment(OMRPORTLIB, "Child 1: Created file\n");

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead | EsOpenWrite | EsOpenSync, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, mappingName, OMRPORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	outputComment(OMRPORTLIB, "Child 1: Mapped file, readable text = \"%s\"\n", mapAddr + 30);

	omrfile_create_status_file(OMRPORTLIB, "omrmmap_test5_child_1_mapped_file", testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, "omrmmap_test5_child_2_changed_file", testName)) {
		SleepFor(1);
	}

	outputComment(OMRPORTLIB, "Child 1: Child 2 has changed file, readable text = \"%s\"\n", mapAddr + 30);

	if (strcmp(mapAddr + 30, modifiedText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in mapped buffer\n");
	}

	omrmmap_unmap_file(mmapHandle);


	omrfile_create_status_file(OMRPORTLIB, "omrmmap_test5_child_1_checked_file", testName);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

int
omrmmap_test5_child_2(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char *testName = "omrmmap_test5_child_2";

	const char *filename = "mmapTest5.tst";
	const char *mappingName = "C:\\full\\path\\to\\mmapTest5.tst";
	intptr_t fd;
	intptr_t rc;

#define J9MMAP_TEST5_BUFFER_SIZE 100
	const char *readableText = "Here is some readable text in the file";
	const char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, "omrmmap_test5_child_1_mapped_file", testName)) {
		SleepFor(1);
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead | EsOpenWrite | EsOpenSync, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, mappingName, OMRPORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	outputComment(OMRPORTLIB, "Child 2: Mapped file, readable text = \"%s\"\n", mapAddr + 30);

	/* Check mapped area */
	if (strcmp(mapAddr + 30, readableText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Readable string does not match in mapped buffer\n");
	}

	/* Change mapped area */
	strcpy(mapAddr + 30, modifiedText);

	rc = omrmmap_msync(mapAddr, (uintptr_t)fileLength, OMRPORT_MMAP_SYNC_WAIT);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Msync failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
	}

	outputComment(OMRPORTLIB, "Child 2: Changed file, readable text = \"%s\"\n", mapAddr + 30);

	omrfile_create_status_file(OMRPORTLIB, "omrmmap_test5_child_2_changed_file", testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, "omrmmap_test5_child_1_checked_file", testName)) {
		SleepFor(1);
	}

	outputComment(OMRPORTLIB, "Child 2: Child 1 has checked file, exiting\n");

	omrmmap_unmap_file(mmapHandle);


exit:
	return reportTestExit(OMRPORTLIB, testName);
}

/**
* Verify that if mmap has the capability to protect, that we get non-zero from omrmmap_get_region_granularity
*
* Verify that if @ref omrmmap.c::omrmmap_protect "omrmmap_protect()" returns  OMRPORT_MMAP_CAPABILITY_PROTECT,
* 	that @ref omrmmap::omrmmap_get_region_granularity "omrmmap_get_region_granularity()" does not return 0
*/
TEST(PortMmapTest, mmap_test6)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test6";

	const char *filename = "mmapTest6.tst";
	intptr_t fd;
	intptr_t rc;

	char buffer[J9MMAP_PROTECT_TESTS_BUFFER_SIZE ];
	int i = 0;
	const char *readableText = "Here is some readable text in the file";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	int32_t mmapCapabilities = omrmmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	if (!(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_WRITE) || !(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_MSYNC)) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_WRITE or _MSYNC not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Initialize buffer to be written */
	for (i = 0; i < J9MMAP_PROTECT_TESTS_BUFFER_SIZE ; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);

	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_write(fd, buffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(fd);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, NULL, OMRPORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	/* This is the heart of the test */
	if (mmapCapabilities & OMRPORT_MMAP_CAPABILITY_PROTECT) {
		if (omrmmap_get_region_granularity(mapAddr) == 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap claims to be capable of PROTECT, however, region granularity is 0\n");
			goto exit;
		}
		rc = omrmmap_protect(mapAddr, (uintptr_t)fileLength, OMRPORT_PAGE_PROTECT_READ | OMRPORT_PAGE_PROTECT_WRITE);
		if ((rc & OMRPORT_PAGE_PROTECT_NOT_SUPPORTED) != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap is claims to be capable of PROTECT, however, omrmmap_protect() returns OMRPORT_PAGE_PROTECT_NOT_SUPPORTED\n");
			goto exit;
		}

	} else {
		rc = omrmmap_protect(mapAddr, (uintptr_t)fileLength, OMRPORT_PAGE_PROTECT_READ | OMRPORT_PAGE_PROTECT_WRITE);
		if ((rc & OMRPORT_PAGE_PROTECT_NOT_SUPPORTED) == 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap is not capable of PROTECT, however, omrmmap_protect() does not return OMRPORT_PAGE_PROTECT_NOT_SUPPORTED\n");
			goto exit;
		}
	}

	/* For Windows CE we should really close th mapping handle after the unmap */
	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	omrmmap_unmap_file(mmapHandle);

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify calling protect on memory aquired using mmap.
 *
 * Verify @ref omrmmap.c::omrmmap_protect "omrmmap_protect()" with
 * OMRPORT_PAGE_PROTECT_READ allows the
 * memory to be read.
 */
TEST(PortMmapTest, mmap_test7)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test7";

	const char *filename = "mmapTest7.tst";
	intptr_t fd;
	intptr_t rc;

#define J9MMAP_TEST1_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST1_BUFFER_SIZE];
	int i = 0;
	const char *readableText = "Here is some readable text in the file";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	int32_t mmapCapabilities = omrmmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	if ((mmapCapabilities & OMRPORT_MMAP_CAPABILITY_PROTECT) == 0) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		goto exit;
	}

	if ((mmapCapabilities & OMRPORT_MMAP_CAPABILITY_READ) == 0) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_READ not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Initialize buffer to be written */
	for (i = 0; i < J9MMAP_TEST1_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);

	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_write(fd, buffer, J9MMAP_TEST1_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(fd);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, NULL, OMRPORT_MMAP_FLAG_READ, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	omrmmap_protect(mapAddr, (uintptr_t)fileLength, OMRPORT_PAGE_PROTECT_READ);

	/* For Windows CE we should really close th mapping handle after the unmap */
	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(mapAddr + 30, readableText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Strings do not match in mapped area\n");
	}

	omrmmap_unmap_file(mmapHandle);

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify calling protect on memory aquired using mmap.
 *
 * Verify @ref omrmmap.c::omrmmap_protect "omrmmap_protect()" with
 * OMRPORT_PAGE_PROTECT_READ | OMRPORT_PAGE_PROTECT_WRITE allows the
 * memory to be read from and written to.
 */
TEST(PortMmapTest, mmap_test8)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test8";

	const char *filename = "mmapTest8.tst";
	intptr_t fd;
	intptr_t rc;

	char buffer[J9MMAP_PROTECT_TESTS_BUFFER_SIZE ];
	char updatedBuffer[J9MMAP_PROTECT_TESTS_BUFFER_SIZE ];
	int i = 0;
	const char *readableText = "Here is some readable text in the file";
	const char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	int32_t mmapCapabilities = omrmmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	if ((mmapCapabilities & OMRPORT_MMAP_CAPABILITY_PROTECT) == 0) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		goto exit;
	}

	if ((mmapCapabilities & OMRPORT_MMAP_CAPABILITY_WRITE) == 0) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_WRITE not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Initialize buffer to be written */
	for (i = 0; i < J9MMAP_PROTECT_TESTS_BUFFER_SIZE ; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);

	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_write(fd, buffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(fd);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, NULL, OMRPORT_MMAP_FLAG_WRITE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	(void)omrmmap_protect(mapAddr, (uintptr_t)fileLength, OMRPORT_PAGE_PROTECT_READ | OMRPORT_PAGE_PROTECT_WRITE);

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	strcpy(mapAddr + 30, modifiedText);

	omrmmap_unmap_file(mmapHandle);

	fd = omrfile_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_read(fd, updatedBuffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer + 30, modifiedText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify calling protect on memory aquired using mmap.
 *
 * Verify @ref omrmmap.c::omrmmap_protect "omrmmap_protect()" with
 * OMRPORT_PAGE_PROTECT_READ causes us to crash when writing to the protected region,
 * and that we are once again able to write when we set it back
 *  to OMRPORT_PAGE_PROTECT_READ | OMRPORT_PAGE_PROTECT_READ
 */
TEST(PortMmapTest, mmap_test9)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test9";

	const char *filename = "mmapTest9.tst";
	intptr_t fd;
	intptr_t rc;

	char buffer[J9MMAP_PROTECT_TESTS_BUFFER_SIZE ];
	char updatedBuffer[J9MMAP_PROTECT_TESTS_BUFFER_SIZE ];
	int i = 0;
	const char *readableText = "Here is some readable text in the file";
	const char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	int32_t mmapCapabilities = omrmmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;
	uint32_t signalHandlerFlags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_SIGSEGV | OMRPORT_SIG_FLAG_SIGBUS;
	struct simpleHandlerInfo handlerInfo;
	struct protectedWriteInfo writerInfo;
	int32_t protectResult;
	uintptr_t result = 0;

	reportTestEntry(OMRPORTLIB, testName);

	if ((mmapCapabilities & OMRPORT_MMAP_CAPABILITY_PROTECT) == 0) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		goto exit;
	}

	if ((mmapCapabilities & OMRPORT_MMAP_CAPABILITY_WRITE) == 0) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_WRITE not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* Initialize buffer to be written */
	for (i = 0; i < J9MMAP_PROTECT_TESTS_BUFFER_SIZE ; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);

	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_write(fd, buffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(fd);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, NULL, OMRPORT_MMAP_FLAG_WRITE | OMRPORT_MMAP_FLAG_SHARED, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	/* set the region to read only */
	rc = omrmmap_protect(mapAddr, (uintptr_t)fileLength, OMRPORT_PAGE_PROTECT_READ);
	if (rc != 0) {
		if (rc == OMRPORT_PAGE_PROTECT_NOT_SUPPORTED) {
			outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		} else {
			lastErrorMessage = (char *)omrerror_last_error_message();
			lastErrorNumber = omrerror_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrmmap_protect() failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
		}
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	/* try to write to region that was set to read only */
	if (!omrsig_can_protect(signalHandlerFlags)) {
		outputComment(OMRPORTLIB, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {

		/* protectedWriter should crash */
		handlerInfo.testName = testName;
		writerInfo.text = modifiedText;
		writerInfo.shouldCrash = 1;
		writerInfo.address = mapAddr + 30;

		outputComment(OMRPORTLIB, "Writing to readonly region - we should crash\n");
		protectResult = omrsig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);

		if (protectResult == OMRPORT_SIG_EXCEPTION_OCCURRED) {
			/* test passed */
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Able to write to read only region\n");
		}
	}

	/* set the region back to be writeable */
	(void)omrmmap_protect(mapAddr, (uintptr_t)fileLength, OMRPORT_PAGE_PROTECT_READ | OMRPORT_PAGE_PROTECT_WRITE);

	/* verify that we can now write to the file */
	strcpy(mapAddr + 30, modifiedText);

	omrmmap_unmap_file(mmapHandle);

	fd = omrfile_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_read(fd, updatedBuffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer + 30, modifiedText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify the page size is correct - create a file over two pages, protect the first and
 * then write to the second page (should be ok) and the last byte of the first page (should fail).
 */
TEST(PortMmapTest, mmap_test10)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test10";
	const char *filename = "mmapTest10.tst";
	intptr_t fd;
	intptr_t rc;

	char buffer[128 ];
	char updatedBuffer[128 ];
	uintptr_t i = 0;
	const char *readableText = "Here is some readable text in the file";
	const char *modifiedText = "This text is modified";
	int64_t fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	int32_t mmapCapabilities = omrmmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;
	uint32_t signalHandlerFlags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_SIGSEGV | OMRPORT_SIG_FLAG_SIGBUS;
	struct simpleHandlerInfo handlerInfo;
	struct protectedWriteInfo writerInfo;
	int32_t protectResult;
	uintptr_t result = 0;
	int32_t bufferSize = 128;
	uintptr_t pageSize;

	reportTestEntry(OMRPORTLIB, testName);

	if ((mmapCapabilities & OMRPORT_MMAP_CAPABILITY_PROTECT) == 0) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		goto exit;
	}

	if ((mmapCapabilities & OMRPORT_MMAP_CAPABILITY_WRITE) == 0) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_WRITE not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* How big do pages claim to be? */
	pageSize = omrmmap_get_region_granularity(mapAddr);
	outputComment(OMRPORTLIB, "Page size is reported as %d\n", pageSize);
	/* Initialize buffer to be written - 128 chars with a string right in the middle */
	for (i = 0; i < 128 ; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);


	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	i = 0;
	while (i < (pageSize * 2)) {
		rc = omrfile_write(fd, buffer, bufferSize);
		if (-1 == rc) {
			lastErrorMessage = (char *)omrerror_last_error_message();
			lastErrorNumber = omrerror_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
			omrfile_close(fd);
			goto exit;
		}
		i += bufferSize;
	}
	outputComment(OMRPORTLIB, "Written out the buffer %d times to fill two pages\n", i / bufferSize);

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	if ((uint64_t)fileLength != pageSize * 2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File is wrong length, should be %d but is %d\n", pageSize * 2, fileLength);
	}

	fd = omrfile_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, NULL, OMRPORT_MMAP_FLAG_WRITE | OMRPORT_MMAP_FLAG_SHARED, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	outputComment(OMRPORTLIB, "File mapped at location %d\n", mapAddr);
	outputComment(OMRPORTLIB, "Protecting first page\n");
	/* set the region to read only */
	rc = omrmmap_protect(mapAddr, (uintptr_t)fileLength / 2, OMRPORT_PAGE_PROTECT_READ);
	if (rc != 0) {
		if (rc == OMRPORT_PAGE_PROTECT_NOT_SUPPORTED) {
			outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		} else {
			lastErrorMessage = (char *)omrerror_last_error_message();
			lastErrorNumber = omrerror_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrmmap_protect() failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
		}
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	/* try to write to region that was set to read only */
	if (!omrsig_can_protect(signalHandlerFlags)) {
		outputComment(OMRPORTLIB, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {

		/* protectedWriter should crash */
		handlerInfo.testName = testName;
		writerInfo.text = modifiedText;
		writerInfo.shouldCrash = 1;
		writerInfo.address = mapAddr + 30;

		outputComment(OMRPORTLIB, "Writing to readonly region - we should crash\n");
		protectResult = omrsig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);

		if (protectResult == OMRPORT_SIG_EXCEPTION_OCCURRED) {
			/* test passed */
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Able to write to read only region\n");
		}
	}

	/* try to write to region that is still writable */
	if (!omrsig_can_protect(signalHandlerFlags)) {
		outputComment(OMRPORTLIB, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {

		/* protectedWriter should not crash */
		handlerInfo.testName = testName;
		writerInfo.text = modifiedText;
		writerInfo.address = mapAddr + pageSize;
		writerInfo.shouldCrash = 0;

		outputComment(OMRPORTLIB, "Writing to page two region - should be ok\n");
		protectResult = omrsig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);

		if (protectResult == OMRPORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Crashed writing to writable page\n");
		} else {
			/* test passed */
		}
	}

	/* set the region back to be writeable */
	(void)omrmmap_protect(mapAddr, (uintptr_t)fileLength / 2, OMRPORT_PAGE_PROTECT_READ | OMRPORT_PAGE_PROTECT_WRITE);

	/* verify that we can now write to the file */
	strcpy(mapAddr + 30, modifiedText);

	omrmmap_unmap_file(mmapHandle);

	fd = omrfile_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_read(fd, updatedBuffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer + 30, modifiedText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/** Work on a funny boundary (ie. not a 64k boundary, just a 4k one) */
TEST(PortMmapTest, mmap_test11)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test11";
	const char *filename = "mmapTest11.tst";
	intptr_t fd;
	intptr_t rc;

	char buffer[128 ];
	char updatedBuffer[128 ];
	uintptr_t i = 0;
	const char *readableText = "Here is some readable text in the file";
	const char *modifiedText = "This text is modified";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	int32_t mmapCapabilities = omrmmap_capabilities();
	J9MmapHandle *mmapHandle = NULL;
	uint32_t signalHandlerFlags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_SIGSEGV | OMRPORT_SIG_FLAG_SIGBUS;
	struct simpleHandlerInfo handlerInfo;
	struct protectedWriteInfo writerInfo;
	int32_t protectResult;
	uintptr_t result = 0;
	int32_t bufferSize = 128;
	uintptr_t pageSize;

	reportTestEntry(OMRPORTLIB, testName);

	if ((mmapCapabilities & OMRPORT_MMAP_CAPABILITY_PROTECT) == 0) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		goto exit;
	}

	if ((mmapCapabilities & OMRPORT_MMAP_CAPABILITY_WRITE) == 0) {
		outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_WRITE not supported on this platform, bypassing test\n");
		goto exit;
	}

	/* How big do pages claim to be? */
	pageSize = omrmmap_get_region_granularity(mapAddr);
	outputComment(OMRPORTLIB, "Page size is reported as %d\n", pageSize);
	/* Initialize buffer to be written - 128 chars with a string right in the middle */
	for (i = 0; i < 128 ; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);


	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	i = 0;
	while (i < (pageSize * 3)) {
		rc = omrfile_write(fd, buffer, bufferSize);
		if (-1 == rc) {
			lastErrorMessage = (char *)omrerror_last_error_message();
			lastErrorNumber = omrerror_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
			omrfile_close(fd);
			goto exit;
		}
		i += bufferSize;
	}
	outputComment(OMRPORTLIB, "Written out the buffer %d times to fill three pages\n", i / bufferSize);

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	if ((uint64_t)fileLength != pageSize * 3) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File is wrong length, should be %d but is %d\n", pageSize * 2, fileLength);
	}

	fd = omrfile_open(filename, EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, NULL, OMRPORT_MMAP_FLAG_WRITE | OMRPORT_MMAP_FLAG_SHARED, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	outputComment(OMRPORTLIB, "File mapped at location %d\n", mapAddr);
	outputComment(OMRPORTLIB, "Protecting first page 4k into that address\n");
	/* set the region to read only */
	rc = omrmmap_protect(mapAddr + pageSize, pageSize, OMRPORT_PAGE_PROTECT_READ);
	if (rc != 0) {
		if (rc == OMRPORT_PAGE_PROTECT_NOT_SUPPORTED) {
			outputComment(OMRPORTLIB, "OMRPORT_MMAP_CAPABILITY_PROTECT not supported on this platform, bypassing test\n");
		} else {
			lastErrorMessage = (char *)omrerror_last_error_message();
			lastErrorNumber = omrerror_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrmmap_protect() failed: lastErrorNumber=%d, lastErrorMessage=%s\n", lastErrorNumber, lastErrorMessage);
		}
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}
	/* Checking we can write into the bit before the locked page */
	strcpy(mapAddr + 30, modifiedText);

	/* try to write to region that was set to read only */
	if (!omrsig_can_protect(signalHandlerFlags)) {
		outputComment(OMRPORTLIB, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {

		/* protectedWriter should crash */
		handlerInfo.testName = testName;
		writerInfo.text = modifiedText;
		writerInfo.shouldCrash = 1;
		writerInfo.address = mapAddr + 30 + pageSize;

		outputComment(OMRPORTLIB, "Writing to readonly region - we should crash\n");
		protectResult = omrsig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);

		if (protectResult == OMRPORT_SIG_EXCEPTION_OCCURRED) {
			/* test passed */
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Able to write to read only region\n");
		}
	}

	/* try to write to region that is still writable */
	if (!omrsig_can_protect(signalHandlerFlags)) {
		outputComment(OMRPORTLIB, "Signal handling framework not available, can't test readonly functionality without crashing, bypassing test");
		goto exit;
	} else {

		/* protectedWriter should not crash */
		handlerInfo.testName = testName;
		writerInfo.text = modifiedText;
		writerInfo.address = mapAddr + (2 * pageSize);
		writerInfo.shouldCrash = 0;

		outputComment(OMRPORTLIB, "Writing to page two region - should be ok\n");
		protectResult = omrsig_protect(protectedWriter, &writerInfo, simpleHandlerFunction, &handlerInfo, signalHandlerFlags, &result);

		if (protectResult == OMRPORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Crashed writing to writable page\n");
		} else {
			/* test passed */
		}
	}
	strcpy(mapAddr + (2 * pageSize), modifiedText);

	/* set the region back to be writeable */
	(void)omrmmap_protect(mapAddr + pageSize, pageSize, OMRPORT_PAGE_PROTECT_READ | OMRPORT_PAGE_PROTECT_WRITE);

	/* verify that we can now write to the file */
	strcpy(mapAddr + 30, modifiedText);

	omrmmap_unmap_file(mmapHandle);

	fd = omrfile_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for read after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_read(fd, updatedBuffer, J9MMAP_PROTECT_TESTS_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read of file %s after update failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after read failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (strcmp(updatedBuffer + 30, modifiedText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Modified string does not match in updated buffer\n");
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port memory mapping.
 *
 * Verify @ref omrmmap.c::omrmmap_map_file "omrmmap_map_file()" correctly
 * manages the memory counters
 */
TEST(PortMmapTest, mmap_test12)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test12";

	const char *filename = "mmapTest1.tst";
	intptr_t fd;
	intptr_t rc;

#define J9MMAP_TEST1_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST1_BUFFER_SIZE];
	int i = 0;
	const char *readableText = "Here is some readable text in the file";
	I_64 fileLength = 0;
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	J9MmapHandle *mmapHandle = NULL;

	/* Values for tracking memory usage */
	uintptr_t initialBytes;
	uintptr_t initialBlocks;
	uintptr_t finalBytes;
	uintptr_t finalBlocks;

	reportTestEntry(OMRPORTLIB, testName);

	/* Initialize buffer to be written */
	for (i = 0; i < J9MMAP_TEST1_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);

	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_write(fd, buffer, J9MMAP_TEST1_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(fd);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fileLength = omrfile_length(filename);
	if (-1 == fileLength) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Getting length of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	getPortLibraryMemoryCategoryData(OMRPORTLIB, &initialBlocks, &initialBytes);

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, (uintptr_t)fileLength, NULL, OMRPORT_MMAP_FLAG_READ, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	/* For Windows CE we should really close th mapping handle after the unmap */
	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	getPortLibraryMemoryCategoryData(OMRPORTLIB, &finalBlocks, &finalBytes);

	/* Category tests aren't entirely straightforward - because the mmap implementation may have done other allocations as part of the map.*/
	if (finalBlocks <= initialBlocks) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Block count did not increase as expected. initialBlocks=%zu, finalBlocks=%zu\n", initialBlocks, finalBlocks);
	}

	if (finalBytes < (initialBytes + fileLength)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Byte count did not increase as expected. initialBytes=%zu, finalBytes=%zu, fileLength=%lld\n", initialBytes, finalBytes, fileLength);
	}

	if (strcmp(mapAddr + 30, readableText) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Strings do not match in mapped area\n");
	}

	omrmmap_unmap_file(mmapHandle);

	getPortLibraryMemoryCategoryData(OMRPORTLIB, &finalBlocks, &finalBytes);

	if (finalBlocks != initialBlocks) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Block count did not decrease as expected. initialBlocks=%zu, finalBlocks=%zu\n", initialBlocks, finalBlocks);
	}

	if (finalBytes != initialBytes) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Byte count did not decrease as expected. initialBytes=%zu, finalBytes=%zu\n", initialBytes, finalBytes);
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port memory mapping.
 *
 * Verify @ref omrmmap.c::omrmmap_map_file "omrmmap_map_file()" correctly
 * maps the whole file when the size argument is 0.
 */
TEST(PortMmapTest, mmap_test13)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_test13";

	const char *filename = "mmapTest13.tst";
	intptr_t fd;
	intptr_t rc;

#define J9MMAP_TEST13_BUFFER_SIZE 100
	char buffer[J9MMAP_TEST13_BUFFER_SIZE];
	int i = 0;
	const char *readableText = "Here is some readable text in the file";
	char *mapAddr = NULL;
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	J9MmapHandle *mmapHandle = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	/* Initialize buffer to be written */
	for (i = 0; i < J9MMAP_TEST13_BUFFER_SIZE; i++) {
		buffer[i] = (char)i;
	}
	strcpy(buffer + 30, readableText);

	(void)omrfile_unlink(filename);

	fd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	rc = omrfile_write(fd, buffer, J9MMAP_TEST13_BUFFER_SIZE);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(fd);
		goto exit;
	}

	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	fd = omrfile_open(filename, EsOpenRead, 0660);
	if (-1 == fd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s for mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	mmapHandle = (J9MmapHandle *)omrmmap_map_file(fd, 0, 0, NULL, OMRPORT_MMAP_FLAG_READ, OMRMEM_CATEGORY_PORT_LIBRARY);
	if ((NULL == mmapHandle) || (NULL == mmapHandle->pointer)) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Mmap_map_file of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	mapAddr = (char *)mmapHandle->pointer;

	/* For Windows CE we should really close th mapping handle after the unmap */
	rc = omrfile_close(fd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s after mapping failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
	}

	if (0 != strcmp(mapAddr + 30, readableText)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Strings do not match in mapped area\n");
	}

	omrmmap_unmap_file(mmapHandle);

exit:
	reportTestExit(OMRPORTLIB, testName);
}

#define TEST_BUFF_LEN 0x10000
TEST(PortMmapTest, mmap_testDontNeed)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrmmap_testDontNeed";
	uint32_t *testData;
	uint32_t cursor = 0;

	reportTestEntry(OMRPORTLIB, testName);
	testData = (uint32_t *)omrmem_allocate_memory(TEST_BUFF_LEN * sizeof(uint32_t), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == testData) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "memory allocation failed\n");
	}
	for (cursor = 0; cursor < TEST_BUFF_LEN; ++cursor) {
		testData[cursor] = 3 * cursor;
	}
	omrmmap_dont_need(testData, sizeof(testData));
	for (cursor = 0; cursor < TEST_BUFF_LEN; ++cursor) {
		if (3 * cursor != testData[cursor]) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Test buffer corrupted at index %d\n", cursor);
			break;
		}
	}
	omrmem_free_memory(testData);
	reportTestExit(OMRPORTLIB, testName);
}

int32_t
omrmmap_runTests(struct OMRPortLibrary *portLibrary, char *argv0, char *omrmmap_child)
{
	if (omrmmap_child != NULL) {
		if (strcmp(omrmmap_child, "omrmmap_test5_1") == 0) {
			return omrmmap_test5_child_1(portLibrary);
		} else if (strcmp(omrmmap_child, "omrmmap_test5_2") == 0) {
			return omrmmap_test5_child_2(portLibrary);
		}
	}
	return 0;
}
