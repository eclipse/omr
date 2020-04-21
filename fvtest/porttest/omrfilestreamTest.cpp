/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library filestream.
 *
 * Exercise the API for port library filestream operations.  These functions
 * can be found in the file @ref omrfilestream.c
 *
 * @note port library file system operations are not optional in the port library table.
 * @note Some file system operations are only available if the standard configuration of the port
 * library is used.
 */
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if defined(OMR_OS_WINDOWS)
#include <direct.h>
#include <windows.h> /* for MAX_PATH */
#endif /* defined(OMR_OS_WINDOWS) */

#include "omrcfg.h"
#include "omrport.h"
#include "testHelpers.hpp"
#include "testProcessHelpers.hpp"

/**
 * CRLFNEWLINES will be defined when we require changing newlines from '\n' to '\r\n'
 */
#undef CRLFNEWLINES
#if defined(OMR_OS_WINDOWS)
#define CRLFNEWLINES
#endif /* defined(OMR_OS_WINDOWS) */

/**
 * Write all data in the buffer. Will write in a loop until all data is sent. If an error occurs,
 * it will stop writing, output an error message, and stop writing.
 */
static intptr_t
filestream_write_all(struct OMRPortLibrary *portLibrary, const char *testName, OMRFileStream *fileStream, const char *output, intptr_t nbytes)
{
	intptr_t rc = 0;
	intptr_t bytesRemaining = nbytes;

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	while (bytesRemaining > 0) {
		rc = omrfilestream_write(fileStream, output + (nbytes - bytesRemaining), bytesRemaining);
		if (rc < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write() returned %d", rc);
			nbytes = rc;
			break;
		}
		bytesRemaining -= rc;
	}

	return nbytes;
}


/**
 * Write all data in the buffer. Will write in a loop until all data is sent. If an error occurs, it
 * will stop writing, output an error message, and stop writing.
 */
static intptr_t
file_write_all(struct OMRPortLibrary *portLibrary, const char *testName, intptr_t file, const char *output, intptr_t nbytes)
{
	intptr_t rc = 0;
	intptr_t bytesRemaining = nbytes;

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	while (bytesRemaining > 0) {
		rc = omrfile_write(file, output + (nbytes - bytesRemaining), bytesRemaining);
		if (rc < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_write() returned %d", rc);
			nbytes = rc;
			break;
		}
		bytesRemaining -= rc;
	}

	return nbytes;
}

/**
 * Read until the supplied buffer is full, or end of file is reached.  Returns the number of bytes
 * written to the buffer on success (0 bytes is success), or negative error code on failure.
 */
static intptr_t
file_read_all(struct OMRPortLibrary *portLibrary, const char *testName, intptr_t file, char *buf, intptr_t nbytes)
{
	intptr_t rc = 0;
	int64_t fileEndOffset = 0;
	int64_t currentOffset = 0;
	intptr_t bytesRead = 0;

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	/* omrfile_read will return -1 if the end of the file is reached.  We want to return 0 if no
	 * bytes were read.
	 */

	/* Find out if we are at the end of the file */
	currentOffset = omrfile_seek(file, 0, EsSeekCur);
	if (currentOffset < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %d", currentOffset);
		return -1;
	}
	fileEndOffset = omrfile_seek(file, 0, EsSeekEnd);
	if (fileEndOffset < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %d", fileEndOffset);
		return -1;
	}
	if (fileEndOffset == currentOffset) {
		/* We are at the end of the file */
		return 0;
	}

	/* Reset to the correct position */
	currentOffset = omrfile_seek(file, currentOffset, EsSeekSet);
	if (fileEndOffset < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %d", currentOffset);
		return -1;
	}

	/* read all data in a loop */
	while (bytesRead < (fileEndOffset - currentOffset)) {
		rc = omrfile_read(file, buf + bytesRead, (nbytes - bytesRead));
		if (rc < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read() returned %d", rc);
			nbytes = rc;
			break;
		}
		bytesRead += rc;
	}

	return bytesRead;
}

/**
 * Verify port library properly setup to run filestream tests. Check to make sure all the functions
 * are in the function table.
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on failure.
 */
TEST(PortFileStreamTest, omrfilestream_test_function_table)
{
	const char* testName = "omrfilestream_test_function_table";

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	if (NULL == OMRPORTLIB->filestream_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->filestream_startup is NULL\n");
	}
	if (NULL == OMRPORTLIB->filestream_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->filestream_shutdown is NULL\n");
	}
	if (NULL == OMRPORTLIB->filestream_open) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->filestream_open is NULL\n");
	}
	if (NULL == OMRPORTLIB->filestream_close) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->filestream_close is NULL\n");
	}
	if (NULL == OMRPORTLIB->filestream_write) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->filestream_write is NULL\n");
	}
	if (NULL == OMRPORTLIB->filestream_vprintf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->filestream_vprintf is NULL\n");
	}
	if (NULL == OMRPORTLIB->filestream_printf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->filestream_printf is NULL\n");
	}
	if (NULL == OMRPORTLIB->filestream_sync) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->filestream_sync is NULL\n");
	}
	if (NULL == OMRPORTLIB->filestream_setbuffer) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->filestream_setbuffer is NULL\n");
	}
	if (NULL == OMRPORTLIB->filestream_fdopen) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->filestream_fdopen is NULL\n");
	}
	if (NULL == OMRPORTLIB->filestream_fileno) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->filestream_fileno is NULL\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}


/**
 * @internal
 * Verify that file open flags and modes are working correctly.  This will create the file, open and
 * close the file twice, and verify that the mode and flags are set.
 *
 * @param[in] portLibrary The port library under test.
 * @param[in] testName The name of the test currently running
 * @param[in] pathName The path of the temporary file used in this test.
 * @param[in] flags Open flags for the filestream
 * @param[in] mode Open mode flags for the filestream
 */
static void
openFlagTest(struct OMRPortLibrary *portLibrary, const char *testName, const char* pathName, int32_t flags, int32_t mode)
{
	OMRFileStream *file;
	J9FileStat buf;
	intptr_t rc;

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	/* Unlink the file */
	omrfile_unlink(pathName);

	/* The file should not exist */
	rc = omrfile_stat(pathName, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, -1);
	}

	/* Open, should succeed */
	file = omrfilestream_open(pathName, flags, mode);
	if (NULL == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto exit;
	}

	/* Verify the filesystem sees it */
	rc = omrfile_stat(pathName, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/* Close the file, should succeed */
	rc = omrfilestream_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/* The file should still exist */
	rc = omrfile_stat(pathName, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/* Open again, should succeed */
	file = omrfilestream_open(pathName, flags, mode);
	if (NULL == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned %d expected valid file handle\n", -1);
		goto exit;
	}

	/* The file should still exist */
	rc = omrfile_stat(pathName, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/* Close the file references, should succeed */
	rc = omrfilestream_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected %d\n", rc, 0);
		goto exit;
	}

exit:
	/* Delete this file, should succeed */
	rc = omrfile_unlink(pathName);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() returned %d expected %d\n", rc, -1);
	}
}

/**
 * Check that opening a file correctly uses modes. Verify that file open flags and modes are working
 * correctly.  This will create the file, open and close the file twice, and verify that the mode
 * and flags are set.
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on failure.
 */
TEST(PortFileStreamTest, omrfilestream_test_open_modes)
{
	const char *testName = "omrfilestream_test_open_modes";
	const char *path1 = "omrfilestream_test_open_modes_1.tst";
	const char *path2 = "omrfilestream_test_open_modes_2.tst";
	const char *path3 = "omrfilestream_test_open_modes_3.tst";
	intptr_t rc;
	OMRFileStream *file1;
	OMRFileStream *file2;
	J9FileStat buf;

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	/* Make sure none of the files exist prior to starting the test, ignoring error if the file doesn't exist */
	omrfile_unlink(path1);
	omrfile_unlink(path2);
	omrfile_unlink(path3);

	/*
	 * Tests that a file is created properly.
	 */

	/* flags = EsOpenCreate, requires one of Write/Read */
	openFlagTest(portTestEnv->getPortLibrary(), testName, path3, EsOpenCreate | EsOpenWrite, 0666);
	/* flags = EsOpenWrite */
	openFlagTest(portTestEnv->getPortLibrary(), testName, path3, EsOpenWrite | EsOpenCreate, 0666);
	/* flags = EsOpenAppend, needs write and create */
	openFlagTest(portTestEnv->getPortLibrary(), testName, path3, EsOpenAppend | EsOpenCreate| EsOpenWrite, 0666);
	/* flags = EsOpenText */
	openFlagTest(portTestEnv->getPortLibrary(), testName, path3, EsOpenText | EsOpenCreate | EsOpenWrite, 0666);

	/*
	 * Read should not be an acceptable flag
	 */
	file2 = omrfilestream_open(path1, EsOpenRead, 0444);
	if (NULL != file2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned valid file handle, expected failure\n");
		omrfilestream_close(file2);
	}
	file2 = omrfilestream_open(path1, EsOpenWrite | EsOpenRead, 0444);
	if (NULL != file2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned valid file handle, expected failure\n");
		omrfilestream_close(file2);
	}
	file2 = omrfilestream_open(path2, EsOpenCreate | EsOpenRead, 0444);
	if (NULL != file2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned valid file handle, expected failure\n");
		omrfilestream_close(file2);
	}
	file2 = omrfilestream_open(path2, EsOpenCreate | EsOpenWrite | EsOpenRead, 0444);
	if (NULL != file2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned valid file handle, expected failure\n");
		omrfilestream_close(file2);
	}

	/*
	 * Check that opening a non-existent file will fail when not told to create the file
	 */
	file2 = omrfilestream_open(path2, EsOpenWrite, 0666);
	if (NULL != file2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned valid file handle, expected failure\n");
		omrfilestream_close(file2);
	}
	file2 = omrfilestream_open(path2, EsOpenTruncate, 0666);
	if (NULL != file2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned valid file handle, expected failure\n");
		omrfilestream_close(file2);
	}
	file2 = omrfilestream_open(path2, EsOpenText, 0666);
	if (NULL != file2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned valid file handle, expected failure\n");
		omrfilestream_close(file2);
	}
	file2 = omrfilestream_open(path2, EsOpenAppend, 0666);
	if (NULL != file2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned valid file handle, expected failure\n");
		omrfilestream_close(file2);
	}

	/* Check that EsOpenCreate will fail when used with mode 0444 */
	file2 = omrfilestream_open(path2, EsOpenCreate, 0444);
	if (NULL != file2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned valid file handle, expected failure\n");
		omrfilestream_close(file2);
	}

	/* The file should not exist after all of these tests */
	rc = omrfile_stat(path2, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, -1);
	}

	/*
	 * Check EsOpenCreateNew will create a file, and fail if it already exists
	 */

	/* See if the file exists, it shouldn't */
	rc = omrfile_stat(path1, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d expected %d\n", rc, -1);
	}

	/* Create a new file, will fail if the file exists.  Expect success */
	file1 = omrfilestream_open(path1, EsOpenCreateNew | EsOpenWrite, 0666);
	if (NULL == file1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned %d expected file handle\n", -1);
		goto exit;
	}

	/* Verify the filesystem sees it */
	rc = omrfile_stat(path1, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}
	rc = omrfile_attr(path1);
	if (EsIsFile != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_attr() returned %d expected %d\n", rc, EsIsFile);
	}

	/* Try and create this file again, expect failure */
	file2 = omrfilestream_open(path1, EsOpenCreateNew, 0666);
	if (NULL != file2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned valid file handle expected -1\n");
	}

	/* The file should still exist */
	rc = omrfile_stat(path1, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/* Close the valid file handle (from first attempt), expect success */
	rc = omrfilestream_close(file1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected %d\n", rc, 0);
	}

	/* The file should still exist */
	rc = omrfile_stat(path1, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if(buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/*
	 * Cleanup all files created for this test
	 */

	omrfile_unlink(path1);
	rc = omrfile_stat(path1, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, -1);
		omrfile_unlink(path1);
	}

	/* This file should not have been created */
	rc = omrfile_stat(path2, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, -1);
		omrfile_unlink(path2);
	}

	/* This file should have been deleted by openflagtest, but make sure */
	rc = omrfile_stat(path3, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, -1);
		omrfile_unlink(path3);
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify that basic read and write filestream operation are functioning.
 *
 * Verify basic @ref j9file::omrfilestream_write "omrfilestream_write()" operations.
 * Test by writing to the file, reopening it and reading back the contents.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
TEST(PortFileStreamTest, omrfilestream_test_basic_write_then_read)
{
	const char* testName = "omrfilestream_test_basic_read_then_write";
	const char *fileName = "omrfilestream_test_basic_read_then_write.tst";
	char outputLine[] = "Output from test";
	char inputLine[128];
	OMRFileStream *filestream = NULL;
	intptr_t file = -1;
	intptr_t rc = 0;

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	/* Open a new file, write to it, and close the file */
	omrfile_unlink(fileName);

	filestream = omrfilestream_open(fileName, EsOpenWrite | EsOpenCreate, 0666);
	if (NULL == filestream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL, expected valid file handle\n");
		goto unlinkFile;
	}

	rc = filestream_write_all(OMRPORTLIB, testName, filestream, outputLine, sizeof(outputLine));
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "filestream_write_all() returned %d expected %d\n", rc , sizeof(outputLine));
	}

	rc = omrfilestream_close(filestream);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected %d\n", rc ,0);
		goto unlinkFile;
	}

	/* Reopen the same file, read back what we wrote (verifying the contents), and close the file */
	file = omrfile_open(fileName, EsOpenRead, 0444);
	if (file < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned error, expected valid file handle\n", -1);
		goto unlinkFile;
	}

	rc = file_read_all(OMRPORTLIB, testName, file, inputLine, sizeof(inputLine));
	if (rc != sizeof(outputLine)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS,"file_read_all() returned %d, expected %d\n");
		goto cleanupFD;
	}

	if (strcmp(outputLine,inputLine)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching written data \"%s\"\n", inputLine, outputLine);
		goto cleanupFD;
	}

cleanupFD:
	rc = omrfile_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

unlinkFile:
	omrfile_unlink(fileName);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify write appending to a file. Verify @ref omrfilestream::omrfilestream_write "omrfilestream_write()"
 * append operations.
 *
 * This test creates a file, writes ABCDE to it then closes it - reopens it for append and
 * writes FGHIJ to it, fileseeks to start of file and reads 10 chars and hopefully this matches
 * ABCDEFGHIJ...
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on error.
 */
TEST(PortFileStreamTest, omrfilestream_test_write_append)
{
	const char* testName = "omrfilestream_test_write_append";
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	const char *fileName = "omrfilestream_test_write_append.tst";
	char outputLine1[] = "ABCDE";
	char outputLine2[] = "FGHIJ";
	char inputBuffer[] =    "0123456789";
	char expectedOutput[] = "ABCDEFGHIJ";
	OMRFileStream *filestream = NULL;
	intptr_t file = -1;
	intptr_t rc = 0;

	reportTestEntry(OMRPORTLIB, testName);

	omrfile_unlink(fileName);

	/* try opening a "new" file - if exists then should still work */
	filestream = omrfilestream_open(fileName, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	if (NULL == filestream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned %d expected valid file handle\n", -1);
		goto unlinkFile;
	}

	/* now lets write to it */
	filestream_write_all(OMRPORTLIB, testName, filestream, outputLine1, sizeof(outputLine1) - 1);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "filestream_write_all() returned %d expected %d\n", rc , sizeof(outputLine1) - 1);
	}

	rc = omrfilestream_close(filestream);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected %d\n", rc, 0);
	}

	/* reopen for appending */
	filestream = omrfilestream_open(fileName, EsOpenWrite | EsOpenAppend, 0666);
	if (NULL == filestream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned %d expected valid file handle\n", -1);
		goto unlinkFile;
	}

	/* append to the file */
	filestream_write_all(OMRPORTLIB, testName, filestream, outputLine2,  sizeof(outputLine2) - 1);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "filestream_write_all() returned %d expected %d\n", rc , sizeof(outputLine2) - 1);
	}

	rc = omrfilestream_close(filestream);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected 0\n", rc);
	}

	/* Reopen the file, and read what was written */
	file = omrfile_open(fileName, EsOpenRead, 0444);
	if (file < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned %d expected valid file handle\n", -1);
		goto unlinkFile;
	}

	/*  read back what was written */
	rc = file_read_all(OMRPORTLIB, testName, file, inputBuffer, sizeof(inputBuffer));
	if (rc != sizeof(expectedOutput) - 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d, expected %d\n", rc, sizeof(expectedOutput) - 1);
		goto closeFD;
	}

	/* Compare what was written */
	if (strncmp(expectedOutput, inputBuffer, rc)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching expected data \"%s\"\n", inputBuffer, expectedOutput);
	}

closeFD:
	rc = omrfile_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected 0\n", rc);
	}

unlinkFile:
	omrfile_unlink(fileName);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Test file owner and group attributes.
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on error.
 */
TEST(PortFileStreamTest, omrfilestream_test_file_owner_and_group_attributes)
{
	const char* testName = "omrfilestream_test_file_owner_and_group_attributes";
	const char *filename = "omrfilestream_test_file_owner_and_group_attributes.tst";
	OMRFileStream *file = NULL;
	UDATA myUid = 0;
	UDATA myGid = 0;
	UDATA fileGid = 0;
	J9FileStat stat;
	int32_t rc = -1;

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	omrfile_unlink(filename);

	file = omrfilestream_open(filename, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open failed to create file %s: error_last_error_number %i\n", filename, omrerror_last_error_number());
		goto unlinkFile;
	}

	rc = omrfile_stat(filename, 0, &stat);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(%s, ..) returned %d expected %d\n", filename, rc, 0);
	}

	myUid = omrsysinfo_get_euid();
	myGid = omrsysinfo_get_egid();
	fileGid = stat.ownerGid;

	if (myUid != stat.ownerUid) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat.ownerUid is %d expected %d\n", stat.ownerUid, myUid);
	}

	if (myGid != fileGid) {
		portTestEnv->log("unexpected group ID.  Possibly setgid bit set. Comparing to group of the current directory\n");
		rc = omrfile_stat(".", 0, &stat);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(%s, ..) returned %d expected %d\n", ".", rc, 0);
		}
		if (fileGid != stat.ownerGid) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat.ownerGid is %d expected %d\n", fileGid, stat.ownerGid);
		}
	}

	/* Close the file references, should succeed */
	rc = omrfilestream_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

unlinkFile:
	/* Delete this file, should succeed */
	rc = omrfile_unlink(filename);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() returned %d expected %d\n", rc, -1);
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Test permission flags of new files.
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on error.
 */
TEST(PortFileStreamTest, omrfilestream_test_open_permission_flags)
{
	const char* testName = "omrfilestream_test_open_permission_flags";
	const char *filename = "omrfilestream_test_open_permission_flags.tst";
	OMRFileStream *file = NULL;
	intptr_t fd = -1;
	J9FileStat stat;
	intptr_t rc = -1;

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	/*
	 * Test a read-writeable file
	 */
	omrfile_unlink(filename);
	file = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite, 0666);
	if (NULL == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() file open failed\n");
		goto readOnlyTest;
	}
	rc = omrfilestream_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

#if defined(OMR_OS_WINDOWS)
	/*The mode param is ignored by omrfilestream_open, so omrfile_chmod() is called*/
	omrfile_chmod(filename, 0666);
#endif /* defined(OMR_OS_WINDOWS) */
	omrfile_stat(filename, 0, &stat);

	if (stat.perm.isUserWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isUserWriteable != 1'\n");
	}
	if (stat.perm.isUserReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isUserReadable != 1'\n");
	}
#if defined(OMR_OS_WINDOWS)
	if (stat.perm.isGroupWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isGroupWriteable != 1'\n");
	}
	if (stat.perm.isGroupReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isGroupReadable != 1'\n");
	}
	if (stat.perm.isOtherWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isOtherWriteable != 1'\n");
	}
	if (stat.perm.isOtherReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isOtherReadable != 1'\n");
	}
#else /* defined(OMR_OS_WINDOWS) */
	/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
#endif /* defined(OMR_OS_WINDOWS) */

	rc = omrfile_unlink(filename);
	if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink failed: %s\n", filename);
			goto exit;
	}

	/*
	 * Test a readonly file.
	 * omrfilestream_open() will not open a filestream for reading.  This test makes sure that
	 * file permissions won't be destroyed when using omrfilestream_fdopen().
	 */
readOnlyTest:
	fd = omrfile_open(filename, EsOpenCreate | EsOpenRead, 0444);
	if (NULL == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() file open failed\n");
		goto writeOnlyTest;
	}
	file = omrfilestream_fdopen(fd, EsOpenRead);
	if (NULL == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() file open failed\n");
		omrfile_close(fd);
		goto writeOnlyTest;
	}
	rc = omrfilestream_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

#if defined(OMR_OS_WINDOWS)
	/*The mode param is ignored by omrfile_open, so omrfile_chmod() is called*/
	omrfile_chmod(filename, 0444);
#endif /* defined(OMR_OS_WINDOWS) */
	omrfile_stat(filename, 0, &stat);

	if (stat.perm.isUserWriteable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isUserWriteable != 0'\n");
	}
	if (stat.perm.isUserReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isUserReadable != 1'\n");
	}
#if defined(OMR_OS_WINDOWS)
	if (stat.perm.isGroupWriteable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isGroupWriteable != 0'\n");
	}
	if (stat.perm.isGroupReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isGroupReadable != 1'\n");
	}
	if (stat.perm.isOtherWriteable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isOtherWriteable != 0'\n");
	}
	if (stat.perm.isOtherReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isOtherReadable != 1'\n");
	}
#else /* defined(OMR_OS_WINDOWS) */
	/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
#endif /* defined(OMR_OS_WINDOWS) */

	rc = omrfile_unlink(filename);
	if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink failed: %s\n", filename);
			goto exit;
	}

	/*
	 * Test a write only file
	 */
writeOnlyTest:
	file = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite, 0222);
	if (NULL == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() file open failed\n");
		goto exit;
	}
	rc = omrfilestream_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

#if defined(OMR_OS_WINDOWS)
	/*The mode param is ignored by omrfile_open, so omrfile_chmod() is called*/
	omrfile_chmod(filename, 0222);
#endif /* defined(OMR_OS_WINDOWS) */
	omrfile_stat(filename, 0, &stat);

	if (stat.perm.isUserWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isUserWriteable != 1'\n");
	}
#if defined(OMR_OS_WINDOWS)
	/*On windows if a file is write-able, then it is also read-able*/
	if (stat.perm.isUserReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isUserReadable != 1'\n");
	}
#else /* defined(OMR_OS_WINDOWS) */
	if (stat.perm.isUserReadable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isUserReadable != 0'\n");
	}
#endif /* defined(OMR_OS_WINDOWS) */

#if defined(OMR_OS_WINDOWS)
	if (stat.perm.isGroupWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isGroupWriteable != 1'\n");
	}
	/*On windows if a file is write-able, then it is also read-able*/
	if (stat.perm.isGroupReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isGroupReadable != 1'\n");
	}
	if (stat.perm.isOtherWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isOtherWriteable != 1'\n");
	}
	/*On windows if a file is write-able, then it is also read-able*/
	if (stat.perm.isOtherReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.perm.isOtherReadable != 1'\n");
	}
#else /* defined(OMR_OS_WINDOWS) */
	/*umask vary so we can't test 'isGroupWritable', 'isGroupReadable', 'isOtherWritable', or 'isOtherReadable'*/
#endif /* defined(OMR_OS_WINDOWS) */

	rc = omrfile_unlink(filename);
	if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink failed: %s\n", filename);
			goto exit;
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}


#if defined(OMR_OS_WINDOWS)
/**
 * Test a very long file name. For both relative and absolute paths, build up a directory hierarchy
 * that is greater than the system defined MAX_PATH.
 *
 * @param[in] portLibrary The port library under test.
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 *
 * @note that this test is only run on windows, as it is the only OS (encountered so far) that
 * supports file names longer than 256 chars.
 */
TEST(PortFileStreamTest, omrfilestream_test_long_file_name)
{
	const char* testName = "omrfilestream_test_long_file_name";
	const char expectedResult[] = "Try a number - 1Try a string - abcTry a mixture - 1 abc 2";
	char inputBuffer[] =          "Try a number - 1Try a string - abcTry a mixture - 1 abc 2";
	char *longDirName = "\\abcdefghijklmnopqrstuvwx";

#define MIN_LENGTH (MAX_PATH + 1)
#define FILENAME_LENGTH MIN_LENGTH*2 /* double the size to accommodate a file size that is just under twice MIN_LENGTH */
	char filePathName[FILENAME_LENGTH];
	char cwd[FILENAME_LENGTH];
	char *basePaths[2];

	OMRFileStream *file = NULL;
	intptr_t fd = -1;

	intptr_t rc = -1;
	int32_t mkdirRc = -1;
	int32_t unlinkRc = -1;
	int i = 0;

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	/* get the current drive designator for the absolute path*/
	_getcwd(cwd, FILENAME_LENGTH);
	cwd[2] = '\0'; /* now we have something like "c:" */

	basePaths[0] = "."; /* to test a relative path */
	basePaths[1] = cwd; /* to test an absolute path */

	for (i = 0; i < 2; i++) {
		omrstr_printf(filePathName, FILENAME_LENGTH, "%s", basePaths[i]);

		/* build up a file name that is longer than 256 characters,
		 * comprised of directories, each of which are less than 256 characters in length*/
		while (strlen(filePathName) < MIN_LENGTH ) {

			omrstr_printf(filePathName + strlen(filePathName), FILENAME_LENGTH - strlen(filePathName), "%s", longDirName);

			mkdirRc = omrfile_mkdir(filePathName);

			if ((mkdirRc != 0)) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir() failed for dir with length %i: %s\n", strlen(filePathName), filePathName);
				goto unlinkFile;
			}
		}

		/* now append filePathName with the actual filename */
		omrstr_printf(filePathName + strlen(filePathName), FILENAME_LENGTH - strlen(filePathName), "\\%s", testName);

		portTestEnv->log("\ttesting filename: %s\n", filePathName);

		/* can we open and write to the file? */
		file = omrfilestream_open(filePathName, EsOpenCreate | EsOpenWrite, 0666);
		if (NULL == file) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() failed\n");
			goto unlinkFile;
		}

		omrfilestream_printf(file, "Try a number - %d",1);
		omrfilestream_printf(file, "Try a string - %s","abc");
		omrfilestream_printf(file, "Try a mixture - %d %s %d",1,"abc",2);

		rc = omrfilestream_close(file);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected %d\n", rc, 0);
		}

		/* Reopen the same file, read back what we wrote (verifying the contents), and close the file */
		fd = omrfile_open(filePathName, EsOpenRead, 0444);
		if (fd < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned error, expected valid file handle\n", -1);
			goto unlinkFile;
		}

		rc = file_read_all(OMRPORTLIB, testName, fd, inputBuffer, sizeof(inputBuffer));
		if (rc != sizeof(expectedResult)-1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS,"file_read_all() returned %d, expected %d\n", rc, sizeof(expectedResult)-1);
			goto cleanupFD;
		}

		if (strncmp(expectedResult, inputBuffer, rc)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching written data \"%s\"\n", inputBuffer, expectedResult);
		}

cleanupFD:
		rc = omrfile_close(fd);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d, expected %d", rc, 0);
		}

unlinkFile:
		/* delete the file */
		unlinkRc = omrfile_unlink(filePathName);
		if (0 != unlinkRc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink failed: %s\n", filePathName);
		}

		/* Now we need to unwind and delete all the directories we created one by one
		 * 	- we simply delete the base directory because it is not empty.
		 * Do this by stripping actual name of the filename, then each occurrence of longDirName from the full path.
		 *
		 * You know you're done when the filePathName doesn't contain the occurrence of longDirName
		 */
		filePathName[strlen(filePathName) - strlen(testName)] = '\0';

		{
			size_t lenLongDirName = strlen(longDirName);

			while (NULL != strstr(filePathName, longDirName)) {

				unlinkRc = omrfile_unlinkdir(filePathName);
				if (0 != unlinkRc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlinkdir failed: %s\n", filePathName);
				}

				filePathName[strlen(filePathName) - lenLongDirName] = '\0';
			}
		}
	}

#undef FILENAME_LENGTH
#undef MIN_LENGTH

	reportTestExit(OMRPORTLIB, testName);
}
#endif /* defined(OMR_OS_WINDOWS) */

/**
 * Verify flush.  Check to make sure that a flush will write all data to the file.
 *
 * @param[in] portLibrary The port library under test
 * @return TEST_PASS on success, TEST_FAIL on error
 */
TEST(PortFileStreamTest, omrfilestream_test_sync)
{
	const char* testName = "omrfilestream_test_sync";
	const char* fileName = "omrfilestream_test_sync.tst";
	const char outputLine[] = "testing output";
	char inputLine[]         = "xxxxxxxxxxxxxx";
	intptr_t expectedReturnValue = sizeof(outputLine);

	OMRFileStream *writeFile = NULL;
	intptr_t readFile = -1;

	int32_t rc = -1;
	intptr_t readCount = 0;
	intptr_t totalReadCount = 0;

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	omrfile_unlink(fileName);

	writeFile = omrfilestream_open(fileName, EsOpenCreate | EsOpenWrite | EsOpenAppend, 0666);
	if (NULL == writeFile) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned -1 expected valid file handle\n");
		goto unlinkFile;
	}
	readFile = omrfile_open(fileName, EsOpenCreate | EsOpenRead, 0666);
	if (readFile < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned -1 expected valid file handle\n");
		omrfilestream_close(writeFile);
		goto unlinkFile;
	}

	/* Make sure that we can't read anything from the file */
	readCount = file_read_all(OMRPORTLIB, testName, readFile, inputLine, expectedReturnValue);
	if (readCount != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d, expected %d\n", readCount, 0);
		goto closeFile;
	}

	/* write to it */
	readCount = omrfilestream_write(writeFile, outputLine, expectedReturnValue);
	if (readCount != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write() returned %d expected %d\n", readCount, expectedReturnValue);
		goto closeFile;
	}

	/* Make sure its not an error to read from the file, probably nothing was read */
	totalReadCount = file_read_all(OMRPORTLIB, testName, readFile, inputLine, expectedReturnValue);
	if (totalReadCount < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d \n", totalReadCount);
		goto closeFile;
	}

	/* flush so we can *definitely* read from the file */
	rc = omrfilestream_sync(writeFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_sync() returned %d expected %d\n", rc, expectedReturnValue);
		goto closeFile;
	}

	/* lets read back what we wrote, and make sure its accurate */
	readCount = file_read_all(OMRPORTLIB, testName, readFile, inputLine+totalReadCount, expectedReturnValue);
	totalReadCount += readCount;
	if (readCount != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d, expected %d\n", readCount, expectedReturnValue);
		goto closeFile;
	}

	if (strcmp(outputLine, inputLine)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching expected data \"%s\"\n", inputLine, outputLine);
	}

closeFile:
	rc = omrfilestream_close(writeFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected 0\n", rc);
	}
	rc = omrfile_close(readFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected 0\n", rc);
	}

unlinkFile:
	omrfile_unlink(fileName);

	reportTestExit(OMRPORTLIB, testName);
}


/**
 * Verify fdopen and fileno.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
TEST(PortFileStreamTest, omrfilestream_test_fdopen_and_fileno)
{

	const char *testName = "omrfilestream_test_fdopen_and_fileno";
	const char *filename = "omrfilestream_test_fdopen_and_fileno.tst";
	const char output[] = "testing output";
	char input[]  = "xxxxxxxxxxxxxx";
	intptr_t characterCount = sizeof(output);
	OMRFileStream *file = NULL;

	intptr_t fileDescriptor = -1;
	int64_t resultingOffset = -1;
	intptr_t byteCount = -1;
	intptr_t rc = -1;

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	/* ensure there is no stale file */
	omrfile_unlink(filename);

	/*
	 * Open a file, open a filestream on the file, get the file descriptor from the filestream
	 * and make sure that it stays the same
	 */

	/* open a file descriptor */
	fileDescriptor = omrfile_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (fileDescriptor < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open failed to create file descriptor\n");
		goto test2;
	}

	/* get a filestream from the descriptor */
	file = omrfilestream_fdopen(fileDescriptor, EsOpenCreate | EsOpenWrite);
	if (NULL == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_fdopen failed to create file %s: error_last_error_number %i\n", filename, omrerror_last_error_number());
		omrfile_close(fileDescriptor);
		goto test2;
	}


	/*
	 * Get a stream from a filedescriptor, and write to the descriptor, then write to the stream,
	 */

	/* open a file descriptor */
	fileDescriptor = omrfile_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (fileDescriptor < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open failed to create file descriptor\n");
		goto test2;
	}

	/* get a filestream from the descriptor */
	file = omrfilestream_fdopen(fileDescriptor, EsOpenCreate | EsOpenWrite);
	if (NULL == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_fdopen failed to create file %s: error_last_error_number %i\n", filename, omrerror_last_error_number());
		omrfile_close(fileDescriptor);
		goto test2;
	}

	/* write to the file descriptor */
	rc = file_write_all(OMRPORTLIB, testName, fileDescriptor, output, characterCount);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_write_all() returned %d expected %d\n", rc , characterCount);
	}

	/* sync the file descriptor */
	rc = omrfile_sync(fileDescriptor);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_sync() returned < 0");
	}

	/* make sure the file descriptor position is correct after the written text in the file */
	resultingOffset = omrfile_seek(fileDescriptor, 0, EsSeekCur);
	if (resultingOffset != characterCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected %lld\n", resultingOffset, (int64_t) (characterCount - 1));
	}

	/* go back to the begining of the file (the filestream and file share a position) */
	resultingOffset = omrfile_seek(fileDescriptor, 0, EsSeekSet);
	if (resultingOffset != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfiles_seek() returned %lld expected %lld\n", resultingOffset, (int64_t) 0);
		goto test1CloseFile;
	}

	/* write to the file stream. This should overwrite everything written to the file descriptor */
	rc = filestream_write_all(OMRPORTLIB, testName, file, output, characterCount);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "filestream_write_all() returned %d expected %d\n", rc , characterCount);
	}

	/* sync the filesream */
	rc = omrfilestream_sync(file);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_sync() returned < 0");
	}

	/* make sure the file descriptor position is correct after the written text in the file */
	resultingOffset = omrfile_seek(fileDescriptor, 0, EsSeekCur);
	if (resultingOffset != characterCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected %lld\n", resultingOffset, (int64_t) (characterCount - 1));
	}

test1CloseFile:
	/* close the filestream (closes the file descriptor) */
	rc = omrfilestream_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/*
	 * Get a stream from a filedescriptor, write to the stream, write to the filedescriptor.
	 */
test2:
	/* open a file descriptor */
	fileDescriptor = omrfile_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (fileDescriptor < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open failed to create file descriptor\n");
		goto test3;
	}

	/* open a filestream from the file descriptor */
	file = omrfilestream_fdopen(fileDescriptor, EsOpenCreate | EsOpenWrite);
	if (NULL == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_fdopen failed to create file %s: error_last_error_number %i\n", filename, omrerror_last_error_number());
		omrfilestream_close(file);
		goto test3;
	}

	/* write to the stream */
	rc = filestream_write_all(OMRPORTLIB, testName, file, output, characterCount);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "filestream_write_all() returned %d expected %d\n", rc , characterCount);
	}

	/* sync the filestream */
	rc = omrfilestream_sync(file);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_sync() returned < 0");
	}

	/* make sure the file descriptor position is correct after the written text in the file */
	resultingOffset = omrfile_seek(fileDescriptor, 0, EsSeekCur);
	if (resultingOffset != characterCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected %lld\n", resultingOffset, (int64_t) (characterCount - 1));
	}

	/* move to the beginning of the file */
	resultingOffset = omrfile_seek(fileDescriptor, 0, EsSeekSet);
	if (resultingOffset != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfiles_seek() returned %lld expected %lld\n", resultingOffset, (int64_t) 0);
		goto test2CloseFile;
	}

	/* write to the file descriptor */
	rc = file_write_all(OMRPORTLIB, testName, fileDescriptor, output, characterCount);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_write_all() returned %d expected %d\n", rc , characterCount);
	}

	/* sync the file descriptor */
	rc = omrfile_sync(fileDescriptor);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_sync() returned < 0");
	}

	/* make sure the file descriptor is after the written text in the file */
	resultingOffset = omrfile_seek(fileDescriptor, 0, EsSeekCur);
	if (resultingOffset != characterCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected %lld\n", resultingOffset, (int64_t) (characterCount - 1));
	}

test2CloseFile:
	/* close the filestream (closes the file descriptor) */
	rc = omrfilestream_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/*
	 * Get a filestream from the file descriptor, write to the filestream, read from the file descriptor
	 */
test3:
	/* open a file descriptor */
	fileDescriptor = omrfile_open(filename, EsOpenCreate | EsOpenRead | EsOpenWrite, 0666);
	if (fileDescriptor < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open failed to create file descriptor\n");
		goto exit;
	}

	/* open a filestream on the file descriptor */
	file = omrfilestream_fdopen(fileDescriptor,  EsOpenCreate | EsOpenRead | EsOpenWrite);
	if (NULL == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_fd open failed to open fd for: %s: error_last_error_number %i\n", filename, omrerror_last_error_number());
		omrfile_close(fileDescriptor);
		goto exit;
	}

	/* write to the stream */
	rc = filestream_write_all(OMRPORTLIB, testName, file, output, characterCount);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "filestream_write_all() returned %d expected %d\n", rc , characterCount);
	}

	/* sync the filestream */
	rc = omrfilestream_sync(file);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_sync() returned < 0");
	}

	/* make sure the file descriptor position is correct after the written text in the file */
	resultingOffset = omrfile_seek(fileDescriptor, 0, EsSeekCur);
	if (resultingOffset != characterCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected %lld\n", resultingOffset, (int64_t) (characterCount - 1));
	}

	/* move to the beginning of the file */
	resultingOffset = omrfile_seek(fileDescriptor, 0, EsSeekSet);
	if (resultingOffset != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfiles_seek() returned %lld expected %lld\n", resultingOffset, (int64_t) 0);
		goto test3CloseFile;
	}

	/* read one copy of the output from the start of the file */
	byteCount = file_read_all(OMRPORTLIB, testName, fileDescriptor, input, characterCount);
	if (byteCount != characterCount)	{
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d, expected %d\n", rc, characterCount);
		goto test3CloseFile;
	}

	/* make sure the file descriptor position is correct after the written text in the file */
	resultingOffset = omrfile_seek(fileDescriptor, 0, EsSeekCur);
	if (resultingOffset != characterCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected %lld\n", resultingOffset, (int64_t) (characterCount - 1));
		goto test3CloseFile;
	}

	/* make sure input == output */
	if (strcmp(output,input)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching written data \"%s\"\n", output,input);
	}

test3CloseFile:
	/* close the filestream (closes the file descriptor) */
	rc = omrfilestream_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

exit:
	/* Delete this file, should succeed */
	rc = omrfile_unlink(filename);

	reportTestExit(OMRPORTLIB, testName);
}


/**
 * Test stream buffering. This test will write to the stream using different buffering methods. With
 * a different stream to the same file, it will ensure that the proper file buffering works
 * correctly.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
TEST(PortFileStreamTest, omrfilestream_test_buffering)
{
	const char *testName = "omrfilestream_test_buffering";
	const char *filename = "omrfilestream_test_buffering.tst";
	const char output[]  = "testing\noutput";
	char input[]         = "xxxxxxxxxxxxxx";
	int32_t characterCount = sizeof(output);
	char *buffer = NULL;
	intptr_t readFile = -1;
	OMRFileStream *writeFile = NULL;
	intptr_t rc = -1;
	intptr_t readCount = -1;

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	/* Make the buffer large enough to avoid all file writes using full buffering. Reading from the
	 * stream should return nothing. After flushing writes to the file, read the text.
	 */

	/* open a filestream for writing */
	writeFile = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == writeFile) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto test2;
	}

	/* open a file descriptor for reading */
	readFile = omrfile_open(filename, EsOpenRead, 0666);
	if (readFile < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		omrfilestream_close(writeFile);
		goto test2;
	}

	/* allocate a buffer */
	buffer = (char *) omrmem_allocate_memory(256, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == buffer) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrmem_allocate_memory failed to allocate buffer of size 256\n");
		omrfilestream_close(writeFile);
		omrfile_close(readFile);
		goto test2;
	}

	/* use the buffer for the filestream */
	rc = omrfilestream_setbuffer(writeFile, buffer, OMRPORT_FILESTREAM_FULL_BUFFERING, 256);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_setbuffer failed to set the buffer\n");
		goto test1CloseFile;
	}

	/* write to the file, it should all be in the buffer */
	rc = filestream_write_all(OMRPORTLIB, testName, writeFile, output, characterCount);
	if (rc < characterCount) {
		goto test1CloseFile;
	}

	/* read nothing from the file */
	rc = file_read_all(OMRPORTLIB, testName, readFile, input, characterCount);
	if (0 != rc) {
		/* read from the file when all output should be buffered.
		 * Technically speaking this is not an error as support for buffering
		 * is implementation defined */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, 0);
	}

	/* sync the file stream */
	rc = omrfilestream_sync(writeFile);
	if (0 > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_sync() failed: returned %d\n", rc);
		goto test1CloseFile;
	}

	/* read from the file */
	rc = file_read_all(OMRPORTLIB, testName, readFile, input, characterCount);
	if (characterCount > rc) {
		/* read from the file when it should be in the buffer.  Not sure if the c standard guarantees it will respect your buffering wishes */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, characterCount);
		goto test1CloseFile;
	}

	/* make sure input == output */
	if (strcmp(output,input)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching written data \"%s\"\n", output, input);
	}

test1CloseFile:
	rc = omrfilestream_close(writeFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	rc = omrfile_close(readFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	omrmem_free_memory(buffer);

	/* Make the buffer large enough to avoid all file write.
	 * Same thing as above, but do not allocate the buffer that is used.
	 */
test2:
	/* open a filestream for writing */
	writeFile = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == writeFile) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto test3;
	}

	/* open a file descriptor for reading */
	readFile = omrfile_open(filename, EsOpenRead, 0666);
	if (readFile < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		omrfilestream_close(writeFile);
		goto test3;
	}

	/* set the buffer (let the system allocate one) */
	rc = omrfilestream_setbuffer(writeFile, NULL, OMRPORT_FILESTREAM_FULL_BUFFERING, 256);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_setbuffer failed to set the buffer\n");
		goto test2CloseFile;
	}

	/* write to the file */
	filestream_write_all(OMRPORTLIB, testName, writeFile, output, characterCount);
	if (rc < characterCount) {
		goto test2CloseFile;
	}

	/* read nothing from the file */
	rc = file_read_all(OMRPORTLIB, testName, readFile, input, characterCount);
	if (0 != rc) {
		/* read from the file when all output should be buffered.
		 * Technically speaking this is not an error as support for buffering characteristics
		 * is implementation defined */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, 0);
		goto test2CloseFile;
	}

	/* sync the file */
	rc = omrfilestream_sync(writeFile);
	if (0 > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_synced() failed: returned %d\n", rc);
		goto test2CloseFile;
	}

	/* read from the file */
	rc = file_read_all(OMRPORTLIB, testName, readFile, input, characterCount);
	if (rc != characterCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, characterCount);
		goto test2CloseFile;
	}

	/* make sure that input == output */
	if (strcmp(output,input)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching written data \"%s\"\n", output, input);
	}

test2CloseFile:
	rc = omrfilestream_close(writeFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	rc = omrfile_close(readFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	/*
	 * Attempt to line buffer output, checking flushed output. After flushing the entire buffer,
	 * make sure the strings are the same.  Note that there is no guarantee that line buffering
	 * will actually line buffer.  The only guarantee is that all text has been written to the file
	 * after a omrfilestream_sync.
	 */
test3:
	/* open a filestream for writing */
	writeFile = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == writeFile) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto test4;
	}

	/* open a file descriptor for reading */
	readFile = omrfile_open(filename, EsOpenRead, 0666);
	if (readFile < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		omrfilestream_close(writeFile);
		goto test4;
	}

	rc = omrfilestream_setbuffer(writeFile, NULL, OMRPORT_FILESTREAM_LINE_BUFFERING, 256);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_setbuffer failed to set the buffer\n");
		goto test3CloseFile;
	}

	rc = filestream_write_all(OMRPORTLIB, testName, writeFile, output, characterCount);
	if (rc < characterCount) {
		goto test3CloseFile;
	}

	/* read from the file.  Do not know how much is in the file. */
	readCount = file_read_all(OMRPORTLIB, testName, readFile, input, characterCount);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d success\n", rc);
		goto test3CloseFile;
	}
	/* we may have read something */
	if (readCount > characterCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d, expected <= %d\n", readCount, characterCount);
		goto test3CloseFile;
	}

	rc = omrfilestream_sync(writeFile);
	if (0 > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_synced() failed: returned %d\n", rc);
		goto test3CloseFile;
	}

	rc = file_read_all(OMRPORTLIB, testName, readFile, input+readCount, characterCount);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d success\n", rc);
		goto test3CloseFile;
	}
	/* make sure both reads combined are the correct amount */
	if (characterCount > (rc + readCount)) {
		/* read from the file when it should be in the buffer.  Not sure if the c standard guarantees
		 * it will respect your buffering wishes */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, (characterCount - readCount));
	}

	if (strcmp(output,input)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching written data \"%s\"\n", output, input);
	}

test3CloseFile:
	rc = omrfilestream_close(writeFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	rc = omrfile_close(readFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	/*
	 * Attempt to use no buffering.  Ensure that flushing wont write more characters to the file.
	 */
test4:
	writeFile = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == writeFile) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto test5;
	}

	readFile = omrfile_open(filename, EsOpenCreate | EsOpenRead, 0666);
	if (readFile < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		omrfilestream_close(writeFile);
		goto test5;
	}

	rc = omrfilestream_setbuffer(writeFile, NULL, OMRPORT_FILESTREAM_NO_BUFFERING, 256);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_setbuffer failed to set the buffer\n");
		goto test4CloseFile;
	}

	rc = filestream_write_all(OMRPORTLIB, testName, writeFile, output, characterCount);
	if (rc < characterCount) {
		goto test4CloseFile;
	}

	rc = file_read_all(OMRPORTLIB, testName, readFile, input, characterCount);
	if (characterCount != rc) {
		/* read from the file when it should be in the buffer.  Not sure if the c standard guarantees it will respect your buffering wishes */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, characterCount);
		goto test4CloseFile;
	}

	/* Should have been able to read it all in at once */
	if (strcmp(output,input)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching written data \"%s\"\n", output, input);
		goto test4CloseFile;
	}

	rc = omrfilestream_sync(writeFile);
	if (0 > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_synced() failed: returned %d\n", rc);
		goto test4CloseFile;
	}

	/* Read nothing from the file */
	rc = file_read_all(OMRPORTLIB, testName, readFile, input, characterCount);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, 0);
		goto test4CloseFile;
	}

test4CloseFile:
	rc = omrfilestream_close(writeFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	rc = omrfile_close(readFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	/* Test full buffering with omrfilestream_setbuffer.  Make the buffer large enough to avoid all
	 * file writes using full buffering. Reading from the stream should return nothing. Flush the
	 * file write, and then read the text.
	 */
test5:
	writeFile = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == writeFile) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto test6;
	}

	readFile = omrfile_open(filename, EsOpenCreate | EsOpenRead, 0666);
	if (readFile < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		omrfilestream_close(writeFile);
		goto test6;
	}

	buffer = (char *) omrmem_allocate_memory(256, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == buffer) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrmem_allocate_memory failed to allocate buffer of size 256\n");
		omrfilestream_close(writeFile);
		omrfile_close(readFile);
		goto test6;
	}

	rc = omrfilestream_setbuffer(writeFile, buffer, OMRPORT_FILESTREAM_FULL_BUFFERING, 256);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_setbuffer failed to set the buffer\n");
		goto test5CloseFile;
	}

	rc = filestream_write_all(OMRPORTLIB, testName, writeFile, output, characterCount);
	if (rc < characterCount) {
		goto test5CloseFile;
	}

	rc = file_read_all(OMRPORTLIB, testName, readFile, input, characterCount);
	if (0 != rc) {
		/* read from the file when it should be in the buffer.  Not sure if the c standard guarantees
		 * it will respect your buffering wishes */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, 0);
		goto test5CloseFile;
	}

	rc = omrfilestream_sync(writeFile);
	if (0 > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_synced() failed: returned %d\n", rc);
		goto test5CloseFile;
	}

	rc = file_read_all(OMRPORTLIB, testName, readFile, input, characterCount);
	if (characterCount > rc) {
		/* read from the file when it should be in the buffer.  Not sure if the c standard guarantees it will respect your buffering wishes */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, characterCount);
		goto test5CloseFile;
	}

	if (strcmp(output,input)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching written data \"%s\"\n", output, input);
		goto test5CloseFile;
	}

test5CloseFile:
	rc = omrfilestream_close(writeFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	rc = omrfile_close(readFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	omrmem_free_memory(buffer);

	/* Test buffering with a small buffer.  On Windows, if the buffer size is less than 2, it should
	 * set the minimum buffer size to 2.
	 */
test6:
	writeFile = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == writeFile) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto exit;
	}

	readFile = omrfile_open(filename, EsOpenCreate | EsOpenRead, 0666);
	if (readFile < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		omrfilestream_close(writeFile);
		goto exit;
	}

	rc = omrfilestream_setbuffer(writeFile, NULL, OMRPORT_FILESTREAM_FULL_BUFFERING, 1);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_setbuffer failed to set the buffer\n");
		goto test6CloseFile;
	}

	rc = filestream_write_all(OMRPORTLIB, testName, writeFile, output, characterCount);
	if (rc < characterCount) {
		goto test6CloseFile;
	}

	rc = omrfilestream_sync(writeFile);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_synced() failed: returned %d\n", rc);
		goto test6CloseFile;
	}

	rc = file_read_all(OMRPORTLIB, testName, readFile, input, characterCount);
	if (rc != characterCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, characterCount);
		goto test6CloseFile;
	}

	if (strcmp(output,input)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching written data \"%s\"\n", output, input);
	}

test6CloseFile:
	rc = omrfilestream_close(writeFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	rc = omrfile_close(readFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

exit:
	omrfile_unlink(filename);
	reportTestExit(OMRPORTLIB, testName);
}


/**
 * Verify that omrfilestream_write_text is working.  Try writing using different variables, with
 * a mix of utf-8 characters with different byte-lengths.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
TEST(PortFileStreamTest, omrfilestream_test_text)
{
	const char *testName = "omrfilestream_test_text";
	const char *filename = "omrfilestream_test_text.tst";
	char input[1024];
	OMRFileStream *filestream = NULL;
	intptr_t fileDescriptor = -1;
	intptr_t rc;

	/*
	 * This string is the sequence U+0024, U+00A2, U+20AC followed by newlines.
	 * They are 1 byte, 2 byte, and 3 byte characters, respectively.
	 */

	/* modified utf-8 */
	const char mutf8String [] = "\x24\n\xc2\xa2\n\xe2\x82\xac\n";
	const intptr_t mutf8StringSize = (intptr_t) sizeof(mutf8String) - 1;
#if defined(CRLFNEWLINES)
	const char expectedmutf8 [] =  "\x24\r\n\xc2\xa2\r\n\xe2\x82\xac\r\n";
#else /* defined (CRLFNEWLINES) */
	const char expectedmutf8 [] =  "\x24\n\xc2\xa2\n\xe2\x82\xac\n";
#endif /* defined (CRLFNEWLINES) */
	const intptr_t expectedmutf8Size = sizeof(expectedmutf8) -1;

	/* utf-16 */
#if defined(CRLFNEWLINES)
	const U_16 wideData [] = {0x24, 0x0d, 0x0a, 0xa2, 0x0d, 0x0a, 0x20ac, 0x0d, 0x0a};
#else /* defined (CRLFNEWLINES) */
	const U_16 wideData [] = {0x24, 0x0a, 0xa2, 0x0a, 0x20ac, 0x0a};
#endif /* defined (CRLFNEWLINES) */
	const char *wideString = (const char *)wideData;
	const intptr_t wideStringSize = (intptr_t) sizeof(wideData);

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	/*
	 * MUTF-8 Test
	 */

	omrfile_unlink(filename);
	filestream = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == filestream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto wideTest;
	}

	fileDescriptor = omrfile_open(filename, EsOpenCreate | EsOpenRead, 0666);
	if (fileDescriptor < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned NULL expected valid file handle\n");
		omrfilestream_close(filestream);
		goto wideTest;
	}

	/* correct mutf-8 - basic string */
	rc = omrfilestream_write_text(filestream, mutf8String, mutf8StringSize, J9STR_CODE_MUTF8);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text returned negative error code: %d", rc);
		goto mutf8TestCloseFiles;
	}
	rc = omrfilestream_sync(filestream);
	if (0 > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_synced() failed: returned %d\n", rc);
		goto mutf8TestCloseFiles;
	}

	rc = file_read_all(OMRPORTLIB, testName, fileDescriptor, input, sizeof(input));
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, 0);
		goto mutf8TestCloseFiles;
	}
	input[rc] = '\0';
	if (rc != (expectedmutf8Size)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read returned %d bytes, expected: %d ", rc, expectedmutf8Size);
		goto mutf8TestCloseFiles;
	} else if (strcmp(input, expectedmutf8)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File data \"%s\" not matching expecting \"%s\"\n", input, expectedmutf8);
		goto mutf8TestCloseFiles;
	}

	/* correct mutf-8 - encoded null character */
	rc = omrfilestream_write_text(filestream, "\xc0\x80", 2, J9STR_CODE_MUTF8);
	if (rc != 2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
		goto mutf8TestCloseFiles;
	}
	rc = omrfilestream_sync(filestream);
	if (0 > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_synced() failed: returned %d\n", rc);
		goto mutf8TestCloseFiles;
	}
	rc = file_read_all(OMRPORTLIB, testName, fileDescriptor, input, sizeof(input));
	if (rc != 2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, 0);
		goto mutf8TestCloseFiles;
	}
	if (memcmp(input, "\xc0\x80", 2)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File data \"%s\" not matching expected \"%s\"\n", input, "\xc0\x80");
		goto mutf8TestCloseFiles;
	}

mutf8TestCloseFiles:
	rc = omrfilestream_close(filestream);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected %d\n", rc, 0);
	}
	rc = omrfile_close(fileDescriptor);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}
	omrfile_unlink(filename);

	/*
	 * Wide Test
	 */
wideTest:
	filestream = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == filestream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto platformTest;
	}
	fileDescriptor = omrfile_open(filename, EsOpenCreate | EsOpenRead, 0666);
	if (fileDescriptor < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned NULL expected valid file handle\n");
		omrfilestream_close(filestream);
		goto platformTest;
	}

	/* correct mutf8 - basic string */
	rc = omrfilestream_write_text(filestream, mutf8String, mutf8StringSize, J9STR_CODE_WIDE);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text returned negative error code: %d", rc);
		goto wideTestCloseFiles;
	}
	rc = omrfilestream_sync(filestream);
	if (0 > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_synced() failed: returned %d\n", rc);
		goto wideTestCloseFiles;
	}

	rc = file_read_all(OMRPORTLIB, testName, fileDescriptor, input, sizeof(input));
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, 0);
		goto wideTestCloseFiles;
	}
	input[rc] = '\0';
	if (rc != wideStringSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read returned %d bytes, expected: %d", rc, wideStringSize);
		goto wideTestCloseFiles;
	} else if (memcmp(input, wideString, wideStringSize)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File data \"%s\" not matching expected \"%s\"\n", input, wideString);
		goto wideTestCloseFiles;
	}

	/* bad mutf-8 - malformed utf */
	rc = omrfilestream_write_text(filestream, "\xe2\x82", sizeof("\xe2\x82") -1, J9STR_CODE_WIDE);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
		goto wideTestCloseFiles;
	}

	/* bad mutf-8 - null character */
	rc = omrfilestream_write_text(filestream, "\0", 1, J9STR_CODE_WIDE);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
		goto wideTestCloseFiles;
	}

	/* correct mutf-8 - encoded null character */
	rc = omrfilestream_write_text(filestream, "\xc0\x80", 2, J9STR_CODE_WIDE);
	if (rc != 2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
		goto wideTestCloseFiles;
	}
	rc = omrfilestream_sync(filestream);
	if (0 > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_synced() failed: returned %d\n", rc);
		goto wideTestCloseFiles;
	}
	rc = file_read_all(OMRPORTLIB, testName, fileDescriptor, input, sizeof(input));
	if (rc != 2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, 0);
		goto wideTestCloseFiles;
	}
	if (memcmp(input, "\0\0", 2)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File data \"%s\" not matching expected \"%s\"\n", input, "\0\0");
		goto wideTestCloseFiles;
	}

wideTestCloseFiles:
	rc = omrfilestream_close(filestream);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}
	rc = omrfile_close(fileDescriptor);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}
	omrfile_unlink(filename);


	/*
	 * Platform Test
	 */
platformTest:
	filestream = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == filestream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto exit;
	}
	fileDescriptor = omrfile_open(filename, EsOpenRead, 0666);
	if (fileDescriptor < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned NULL expected valid file handle\n");
		omrfilestream_close(filestream);
		goto exit;
	}

	/* this write can fail.  Some platform encodings do not support mutf-8, they may
	 * only be ascii compatable */
	rc = omrfilestream_write_text(filestream, mutf8String, mutf8StringSize, J9STR_CODE_PLATFORM_RAW);
	if (rc < 0) {
		/* do nothing */
	}
	rc = omrfilestream_sync(filestream);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_synced() failed: returned %d\n", rc);
		goto platformTestCloseFiles;
	}
	rc = file_read_all(OMRPORTLIB, testName, fileDescriptor, input, sizeof(input));
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, 0);
		goto platformTestCloseFiles;
	}

	/* bad mutf-8 - malformed utf */
	rc = omrfilestream_write_text(filestream, "\xe2\x82", sizeof("\xe2\x82") -1, J9STR_CODE_PLATFORM_RAW);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
		goto platformTestCloseFiles;
	}

	/* bad mutf-8 - null character */
	rc = omrfilestream_write_text(filestream, "\0", 1, J9STR_CODE_PLATFORM_RAW);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
		goto platformTestCloseFiles;
	}

	/* correct mutf-8 - encoded null character */
	rc = omrfilestream_write_text(filestream, "\xc0\x80", 2, J9STR_CODE_WIDE);
	if (rc != 2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
		goto platformTestCloseFiles;
	}
	rc = omrfilestream_sync(filestream);
	if (0 > rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_synced() failed: returned %d\n", rc);
		goto platformTestCloseFiles;
	}
	rc = file_read_all(OMRPORTLIB, testName, fileDescriptor, input, sizeof(input));
	if (rc != 2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, 0);
		goto platformTestCloseFiles;
	}
	/* This may fail if the platform encoding does not support \0 */
	if (memcmp(input, "\0\0", 2)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File data \"%s\" not matching expected \"%s\"\n", input, "\0\0");
		goto platformTestCloseFiles;
	}

platformTestCloseFiles:
	rc = omrfilestream_close(filestream);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}
	rc = omrfile_close(fileDescriptor);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

exit:
	omrfile_unlink(filename);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify that text transliteration is working by comparing the results of omrfile_write_text() and
 * omrfilestream_write_text() to each other.
 *
 * The expectation is that omrfile_write_text will have the same output as omrfilestream_write_text()
 * when the toCode is J9STR_CODE_PLATFORM_RAW.  The test then reads in the the files as binary and
 * compares what is in both files.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
TEST(PortFileStreamTest, omrfilestream_test_omrfile_text_comparison)
{
	const char *testName = "omrfilestream_test_omrfile_text_comparison";
	const char *streamPath = "omrfilestream_test_omrfile_text_comparison_stream_text.tst";
	const char *filePath = "omrfilestream_test_omrfile_text_comparison_file_text.tst";
	const char output[] = "abc\ndef";

	const int32_t outputCount = sizeof(output) - 1; /* no trailing NULL */
	char streamInput[512];
	char fileInput[512];
	OMRFileStream *fileStream = NULL;
	intptr_t file = 0;
	intptr_t fileStreamFile = 0;
	intptr_t rc = 0;
	intptr_t rc2 = 0;

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	/* Open both files */
	fileStream = omrfilestream_open(streamPath, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == fileStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto exit;
	}

	file = omrfile_open(filePath, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (-1 == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		omrfilestream_close(fileStream);
		goto exit;
	}

	/* Write text to both files */
	rc = omrfilestream_write_text(fileStream, output, outputCount, J9STR_CODE_PLATFORM_RAW);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() failed: returned %d\n", rc);
	}

	rc = omrfile_write_text(file, output, outputCount);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_write_text() failed: returned %d\n", rc);
	}

	/* Close and reopen both files for reading */
	rc = omrfilestream_close(fileStream);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected %d\n", rc, 0);
	}
	rc = omrfile_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

	fileStreamFile = omrfile_open(streamPath, EsOpenRead, 0666);
	if (fileStreamFile < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned NULL expected valid file handle\n");
		goto exit;
	}
	file = omrfile_open(filePath, EsOpenRead, 0666);
	if (-1 == file) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned NULL expected valid file handle\n");
		omrfile_close(fileStreamFile);
		goto exit;
	}

	/* Read from both, comparing the read back data each time */
	rc = file_read_all(OMRPORTLIB, testName, fileStreamFile, streamInput, sizeof(streamInput));
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc, 0);
	}
	rc2 = file_read_all(OMRPORTLIB, testName, file, fileInput, sizeof(fileInput));
	if (rc2 < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_read_all() returned %d expected %d\n", rc2, 0);
	}

	if (rc != rc2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read on filestream returned %d bytes, while omrfile_read on file returned %d bytes", rc, rc2);
		goto closeFiles;
	} else if (strncmp(fileInput, streamInput, rc)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Stream data \"%s\" not matching file data \"%s\"\n", streamInput, fileInput);
	}

	/* Close both files */
closeFiles:
	rc = omrfile_close(fileStreamFile);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d expected %d\n", rc, 0);
	}
	rc = omrfile_close(file);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

exit:
	omrfile_unlink(streamPath);
	omrfile_unlink(filePath);
	reportTestExit(OMRPORTLIB, testName);
}


/**
 * Verify that printf accepts the proper parameters and writes to the platform encoding.
 * There is no way to read back the characters and verify that they are correct, as not all
 * encoding conversions are reversible.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
TEST(PortFileStreamTest, omrfilestream_test_printf)
{
	const char *testName = "omrfilestream_test_printf";
	const char *filename = "omrfilestream_test_printf.tst";
	OMRFileStream *filestream;
	intptr_t rc;
	size_t i = 0;

	/* Hard coded multi byte character sequence */
	const char utf8text[] = "\x24\n\xC2\xA2\n\xE2\x82\xAC\n%d";

	/* Used to print a string long enough that it won't fit in the buffer */
	char longString[1024];

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	/* Fill long string with printable characters, make sure that there is no embedded '%'character */
	for (i = 0; i < sizeof(longString); i++) {
		longString[i] = (i % (90 - 48)) + 48;
	}
	longString[i-1] = '\0';

	/* Open a file */
	filestream = omrfilestream_open(filename, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL == filestream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto exit;
	}

	/* Print a number to the file */
	omrfilestream_printf(filestream, "Try a number - %d\n", 1);

	/* Print a string to the file */
	omrfilestream_printf(filestream, "Try a string - %s\n", "abc");

	/* Print a mixture to the file */
	omrfilestream_printf(filestream, "Try a mixture - %d %s %d\n", 1, "abc", 2);

	/* Print utf-8 to the file */
	omrfilestream_printf(filestream, utf8text, 2);

	/* Print the long string with printf, it should be read back exactly the same */
	omrfilestream_printf(filestream, longString);

	/* Close the file */
	rc = omrfilestream_close(filestream);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_close() returned %d expected %d\n", rc, 0);
	}

exit:
	omrfile_unlink(filename);

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify the behaviors of writing and reading from invalid filestream, or filestream
 * with a restrictive open mode.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
TEST(PortFileStreamTest, omrfilestream_test_invalid_filestream_access)
{
	const char *testName = "omrfilestream_test_invalid_filestream_access";
	const char *fileName1 = "omrfilestream_test_invalid_filestream_access1.tst";
	const char *fileName2 = "omrfilestream_test_invalid_filestream_access2.tst";
	const char *fileName3 = "omrfilestream_test_invalid_filestream_access3.tst";
	const char *fileName4 = "omrfilestream_test_invalid_filestream_access4.tst";
	const char *fileName5 = "omrfilestream_test_invalid_filestream_access4.tst";
	const char *output = "test output";
	intptr_t rc = 0;

	/* reading and writingg */
	OMRFileStream *validStream = NULL;
	/* read only */
	OMRFileStream *roStream = NULL;
	/* write only */
	OMRFileStream *woStream = NULL;
	/* not enough permissions for reading and writing */
	OMRFileStream *noAccessStream = NULL;
	/* stream which should never be valid */
	OMRFileStream *invalidStream = NULL;
	/* file which should never be valid */
	intptr_t fileDescriptor = -1;

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	omrfile_unlink(fileName1);
	omrfile_unlink(fileName2);

	/*
	 * Open
	 */
	noAccessStream = omrfilestream_open(fileName1, EsOpenWrite | EsOpenTruncate, 0666);
	if (NULL != noAccessStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned non-null, expected invalid file handle\n");
		omrfilestream_close(invalidStream);
	}
	noAccessStream = omrfilestream_open(fileName1, EsOpenTruncate, 0666);
	if (NULL != noAccessStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned non-null, expected invalid file handle\n");
		omrfilestream_close(invalidStream);
	}

	/* Create a stream with no read or write permission.  Create a file with mode 0000.  Must
	 * reopen the file to get the "Permission Denied" error */
	noAccessStream = omrfilestream_open(fileName1, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0000);
	if (NULL == noAccessStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto unlinkFiles;
	}
	rc = omrfilestream_close(noAccessStream);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned negative error code %d\n", rc);
		goto unlinkFiles;
	}
#if !defined(OMR_OS_WINDOWS)
	/* This test has different behavior on windows, and it will allow you to open files even if you
	 * do not have the correct permissions.
	 */
	noAccessStream = omrfilestream_open(fileName1, EsOpenWrite | EsOpenRead | EsOpenTruncate, 0666);
	if (NULL != noAccessStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned valid file handle, expected NULL\n");
	}
#endif /* !defined(OMR_OS_WINDOWS) */


	/*
	 * Close
	 */
	rc = omrfilestream_close(NULL);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned %d, expected error code\n", rc);
	}
	/* Close a filestream with an invalid underlying descriptor */
	fileDescriptor = omrfile_open(fileName5, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fileDescriptor) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned -1, expected valid file descriptor");
	}
	invalidStream = omrfilestream_fdopen(fileDescriptor, EsOpenCreate | EsOpenWrite);
	if (NULL == invalidStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_fdopen() returned NULL expected valid file handle\n");
	}
	rc = omrfile_close(fileDescriptor);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned failed, expected success\n");
	}
	rc = omrfilestream_close(invalidStream);
	if (rc == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned success, expected failure\n");
	}

	/*
	 * Get a some valid filestreams this time (for future testing)
	 */
	validStream = omrfilestream_fdopen(omrfile_open(fileName3, EsOpenCreate | EsOpenRead | EsOpenWrite, 0666), EsOpenCreate | EsOpenRead | EsOpenWrite);
	if (NULL == validStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto unlinkFiles;
	}
	roStream = omrfilestream_fdopen(omrfile_open(fileName3, EsOpenCreate | EsOpenRead, 0666), EsOpenCreate | EsOpenRead);
	if (NULL == roStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto unlinkFiles;
	}
	woStream = omrfilestream_open(fileName4, EsOpenCreate | EsOpenWrite, 0666);
	if (NULL == woStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
		goto unlinkFiles;
	}

	/*
	 * Write
	 */
	rc = omrfilestream_write(NULL, output, sizeof(output));
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write() returned %d, expected error code\n", rc);
	}
	rc = omrfilestream_write(validStream, NULL, sizeof(output));
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write() returned %d, expected error code\n", rc);
	}
	rc = omrfilestream_write(validStream, output, -1);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write() returned %d, expected error code\n", rc);
	}
	rc = omrfilestream_write(roStream, output, sizeof(output));
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write() returned %d, expected error code\n", rc);
	}
	rc = omrfilestream_write(validStream, output, 0);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write() returned %d, expected error code\n", rc);
	}
	omrfilestream_sync(validStream);
	/* Write a filestream with an invalid file descriptor (must not be buffered) */
	fileDescriptor = omrfile_open(fileName5, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fileDescriptor) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned -1, expected valid file descriptor");
		goto writeInvalidFDEnd;
	}
	invalidStream = omrfilestream_fdopen(fileDescriptor, EsOpenCreate | EsOpenWrite);
	if (NULL == invalidStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_fdopen() returned NULL expected valid file handle\n");
	}
	rc = omrfilestream_setbuffer(invalidStream, NULL, OMRPORT_FILESTREAM_NO_BUFFERING, 0);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_setbuffer() failed, expected success\n");
	}
	rc = omrfile_close(fileDescriptor);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned failed, expected success\n");
	}
	rc = omrfilestream_write(invalidStream, output, sizeof(output));
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write() returned success, expected failure\n");
	}
writeInvalidFDEnd:
	/* leaking the filestream here */

	/*
	 * Sync
	 */
	rc = omrfilestream_sync(NULL);
	if (rc == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_sync() returned %d, expected error code\n", rc);
	}
	rc = omrfilestream_sync(roStream);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_sync() returned %d, expected 0\n", rc);
	}
	/* Flush a filestream with an invalid file descriptor (must have something to flush) */
	fileDescriptor = omrfile_open(fileName5, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fileDescriptor) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned -1, expected valid file descriptor");
	}
	invalidStream = omrfilestream_fdopen(fileDescriptor, EsOpenCreate | EsOpenWrite);
	if (NULL == invalidStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_fdopen() returned NULL expected valid file handle\n");
	}
	rc = filestream_write_all(OMRPORTLIB, testName, invalidStream, output, 1);
	if (rc != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write() returned success, expected failure\n");
	}
	rc = omrfile_close(fileDescriptor);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned failed, expected success\n");
	}
	rc = omrfilestream_sync(invalidStream);
	if (rc == 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_sync() returned success, expected failure\n");
	}

	/*
	 * write_text
	 */
	rc = omrfilestream_write_text(NULL, output, sizeof(output), J9STR_CODE_MUTF8);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
	}
	rc = omrfilestream_write_text(validStream, NULL, sizeof(output), J9STR_CODE_MUTF8);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
	}
	rc = omrfilestream_write_text(validStream, output, -1, J9STR_CODE_MUTF8);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
	}
	rc = omrfilestream_write_text(validStream, output, 0, J9STR_CODE_MUTF8);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected 0\n", rc);
	}
	omrfilestream_sync(validStream);
	rc = omrfilestream_write_text(roStream, output, sizeof(output), J9STR_CODE_MUTF8);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
	}
	rc = omrfilestream_write_text(validStream, output, sizeof(output), 200);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned %d, expected error code\n", rc);
	}
	fileDescriptor = omrfile_open(fileName5, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fileDescriptor) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned -1, expected valid file descriptor");
	}
	/* write to an invalid stream (must be unbuffered) */
	invalidStream = omrfilestream_fdopen(fileDescriptor, EsOpenCreate | EsOpenWrite);
	if (NULL == invalidStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_fdopen() returned NULL expected valid file handle\n");
		goto writeTextInvalidFDEnd;
	}
	rc = omrfilestream_setbuffer(invalidStream, NULL, OMRPORT_FILESTREAM_NO_BUFFERING, 0);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_setbuffer() failed, expected success\n");
	}
	rc = omrfile_close(fileDescriptor);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_close() returned failed, expected success\n");
	}
	rc = omrfilestream_write_text(invalidStream, output, sizeof(output), J9STR_CODE_MUTF8);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_write_text() returned success, expected failure\n");
	}
writeTextInvalidFDEnd:

	/*
	 * printf - no error return code for these functions
	 */
	omrfilestream_printf(NULL, "");
	omrfilestream_printf(roStream, "");
	omrfilestream_printf(validStream, NULL);
	omrfilestream_printf(validStream, "%r");

	/*
	 * setbuffer
	 */
	rc = omrfilestream_setbuffer(NULL, NULL, OMRPORT_FILESTREAM_FULL_BUFFERING, 256);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_setbuffer() returned %d, expected error code\n", rc);
	}
	rc = omrfilestream_setbuffer(validStream, NULL, ~OMRPORT_FILESTREAM_FULL_BUFFERING, 256);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_setbuffer() returned %d, expected error code\n", rc);
	}

	/*
	 * fdopen
	 */
	if (NULL != omrfilestream_fdopen(0, ~(EsOpenWrite | EsOpenRead))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_fdopen() did not return expected error code\n", rc);
	}

	/*
	 * fileno
	 */
	rc = omrfilestream_fileno(NULL);
	if (rc != -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_fileno() returned %d, expected invalid file descriptor -1\n", rc);
	}

	/* Close the filestreams */
	omrfilestream_close(validStream);
	omrfilestream_close(roStream);
	omrfilestream_close(woStream);
unlinkFiles:
	omrfile_unlink(fileName1);
	omrfile_unlink(fileName2);
	omrfile_unlink(fileName3);
	omrfile_unlink(fileName4);

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Test to make sure that the standard IO streams are working.  The specific streams are stdin,
 * stdout, and stderr.  Make sure that they will not error when opened and written to.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
TEST(PortFileStreamTest, omrfilestream_test_std_streams)
{
	const char *testName = "omrfilestream_test_std_streams";
	OMRFileStream *fileStream = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	reportTestEntry(OMRPORTLIB, testName);

	/* standard out */
	fileStream = omrfilestream_fdopen(OMRPORT_TTY_OUT, EsOpenWrite);
	if (NULL == fileStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
	}
	omrfilestream_printf(fileStream, "Some text");

	/* standard in */
	fileStream = omrfilestream_fdopen(OMRPORT_TTY_IN, EsOpenRead);
	if (NULL == fileStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
	}

	/* standard error */
	fileStream = omrfilestream_fdopen(OMRPORT_TTY_ERR, EsOpenWrite);
	if (NULL == fileStream) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfilestream_open() returned NULL expected valid file handle\n");
	}
	omrfilestream_printf(fileStream, "Some text");

	/* Cannot free these filestreams (causing a memory leak for the test suite), as that will close
	 * the underlying file descriptors.  In the future the smart thing to do would be to DUP the
	 * file descriptor before opening it as a stream.
	 */

	reportTestExit(OMRPORTLIB, testName);
}
