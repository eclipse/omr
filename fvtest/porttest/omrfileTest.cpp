/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 * @brief Verify port library file system.
 *
 * Exercise the API for port library file system operations.  These functions
 * can be found in the file @ref omrfile.c
 *
 * @note port library file system operations are not optional in the port library table.
 * @note Some file system operations are only available if the standard configuration of the port library is used.
 *
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if defined(OMR_OS_WINDOWS)
#include <direct.h>
#include <windows.h> /* for MAX_PATH */
#endif /* defined(OMR_OS_WINDOWS) */

#include "testHelpers.hpp"
#include "testProcessHelpers.hpp"
#include "omrport.h"

#define J9S_ISGID 02000
#define J9S_ISUID 04000

#define FILE_OPEN_FUNCTION			omrfile_open_helper
#define FILE_CLOSE_FUNCTION			omrfile_close_helper
#define FILE_LOCK_BYTES_FUNCTION	omrfile_lock_bytes_helper
#define FILE_UNLOCK_BYTES_FUNCTION	omrfile_unlock_bytes_helper
#define FILE_READ_FUNCTION			omrfile_read_helper
#define FILE_WRITE_FUNCTION			omrfile_write_helper
#define FILE_SET_LENGTH_FUNCTION	omrfile_set_length_helper
#define FILE_FLENGTH_FUNCTION		omrfile_flength_helper

#define FILE_OPEN_FUNCTION_NAME			(isAsync? "omrfile_blockingasync_open" : "omrfile_open")
#define FILE_CLOSE_FUNCTION_NAME		(isAsync? "omrfile_blockingasync_close" : "omrfile_close")
#define FILE_LOCK_BYTES_FUNCTION_NAME	(isAsync? "omrfile_blockingasync_lock_bytes" : "omrfile_lock_bytes")
#define FILE_UNLOCK_BYTES_FUNCTION_NAME	(isAsync? "omrfile_blockingasync_unlock_bytes" : "omrfile_unlock_bytes")
#define FILE_READ_FUNCTION_NAME			(isAsync? "omrfile_blockingasync_read" : "omrfile_read")
#define FILE_WRITE_FUNCTION_NAME		(isAsync? "omrfile_blockingasync_write" : "omrfile_write")
#define FILE_SET_LENGTH_FUNCTION_NAME	(isAsync? "omrfile_blockingasync_set_length" : "omrfile_set_length")
#define FILE_FLENGTH_FUNCTION_NAME		(isAsync? "omrfile_blockingasync_flength" : "omrfile_flength")

static BOOLEAN isAsync = FALSE;

#define APPEND_ASYNC(testName) (isAsync ?  #testName "_async" : #testName )

class PortFileTest2 : public ::testing::Test
{
protected:
	static void
	TearDownTestCase()
	{
		testFileCleanUp("tfileTest");
		testFileCleanUp("omrfile_test");
	}
};

/**
 * @internal
 * Write to a file
 * @param[in] portLibrary The port library under test
 * @param[in] fd1 File descriptor to write to
 * @param[in] format Format string to write
 * @param[in] ... Arguments for format
 */
static void
fileWrite(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *format, ...)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	va_list args;

	va_start(args, format);
	omrfile_vprintf(fd, format, args);
	va_end(args);
}


intptr_t
omrfile_open_helper(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	return isAsync? omrfile_blockingasync_open(path, flags, mode) : omrfile_open(path, flags, mode);
}

int32_t
omrfile_close_helper(struct OMRPortLibrary *portLibrary, intptr_t fd)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	return isAsync? omrfile_blockingasync_close(fd) : omrfile_close(fd);
}

int32_t
omrfile_lock_bytes_helper(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t lockFlags, uint64_t offset, uint64_t length)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	return isAsync? omrfile_blockingasync_lock_bytes(fd, lockFlags, offset, length) : omrfile_lock_bytes(fd, lockFlags, offset, length);
}

int32_t
omrfile_unlock_bytes_helper(struct OMRPortLibrary *portLibrary, intptr_t fd, uint64_t offset, uint64_t length)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	return isAsync? omrfile_blockingasync_unlock_bytes(fd, offset, length) : omrfile_unlock_bytes(fd, offset, length);
}

intptr_t
omrfile_read_helper(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	return isAsync? omrfile_blockingasync_read(fd, buf, nbytes) : omrfile_read(fd, buf, nbytes);
}

intptr_t
omrfile_write_helper(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	return isAsync? omrfile_blockingasync_write(fd, buf, nbytes) : omrfile_write(fd, buf, nbytes);
}

int32_t
omrfile_set_length_helper(struct OMRPortLibrary *portLibrary, intptr_t fd, I_64 newLength)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	return isAsync? omrfile_blockingasync_set_length(fd, newLength) : omrfile_set_length(fd, newLength);
}

I_64
omrfile_flength_helper(struct OMRPortLibrary *portLibrary, intptr_t fd)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	return isAsync? omrfile_blockingasync_flength(fd) : omrfile_flength(fd);
}



/**
 * Verify port library properly setup to run file tests
 */
TEST_F(PortFileTest2, file_test0)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test0);

	reportTestEntry(OMRPORTLIB, testName);

	/* Some operations are always present, even if the port library does not
	 * support file systems.  Verify these first
	 */

	/* Verify that the file function pointers are non NULL */

	if (FALSE == isAsync) {
		/* omrfile_test1 */
		if (NULL == OMRPORTLIB->file_write) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_write is NULL\n");
		}

		/* omrfile_test7, omrfile_test8 */
		if (NULL == OMRPORTLIB->file_read) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_read is NULL\n");
		}

		/* omrfile_test5, omrfile_test6 */
		if (NULL == OMRPORTLIB->file_open) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_open is NULL\n");
		}

		/* omrfile_test5, omrfile_test6 */
		if (NULL == OMRPORTLIB->file_close) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_close is NULL\n");
		}

		if (NULL == OMRPORTLIB->file_lock_bytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_lock_bytes is NULL\n");
		}

		if (NULL == OMRPORTLIB->file_unlock_bytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_unlock_bytes is NULL\n");
		}

		/* omrfile_test9 */
		if (NULL == OMRPORTLIB->file_set_length) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_set_length is NULL\n");
		}

		if (NULL == OMRPORTLIB->file_flength) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_flength is NULL\n");
		}

		if (NULL == OMRPORTLIB->file_fstat) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_fstat is NULL\n");
		}

		/* Not tested, implementation dependent.  No known functionality.
		 * Startup is private to the portlibary, it is not re-entrant safe
		 */
		if (NULL == OMRPORTLIB->file_startup) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_startup is NULL\n");
		}

		/* Not tested, implementation dependent.  No known functionality */
		if (NULL == OMRPORTLIB->file_shutdown) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_shutdown is NULL\n");
		}

	} else {
		/* omrfile_test1 */
		if (NULL == OMRPORTLIB->file_blockingasync_write) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_write is NULL\n");
		}

		/* omrfile_test7, omrfile_test8 */
		if (NULL == OMRPORTLIB->file_blockingasync_read) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_read is NULL\n");
		}

		/* omrfile_test5, omrfile_test6 */
		if (NULL == OMRPORTLIB->file_blockingasync_open) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_open is NULL\n");
		}

		/* omrfile_test5, omrfile_test6 */
		if (NULL == OMRPORTLIB->file_blockingasync_close) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_close is NULL\n");
		}

		if (NULL == OMRPORTLIB->file_blockingasync_lock_bytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_lock_bytes is NULL\n");
		}

		if (NULL == OMRPORTLIB->file_blockingasync_unlock_bytes) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_unlock_bytes is NULL\n");
		}

		/* omrfile_test9 */
		if (NULL == OMRPORTLIB->file_blockingasync_set_length) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_set_length is NULL\n");
		}

		if (NULL == OMRPORTLIB->file_blockingasync_flength) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_flength is NULL\n");
		}

		/* Not tested, implementation dependent.  No known functionality.
		 * Startup is private to the portlibary, it is not re-entrant safe
		 */
		if (NULL == OMRPORTLIB->file_blockingasync_startup) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_startup is NULL\n");
		}

		/* Not tested, implementation dependent.  No known functionality */
		if (NULL == OMRPORTLIB->file_blockingasync_shutdown) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_blockingasync_shutdown is NULL\n");
		}
	}

	/* omrfile_test2 */
	if (NULL == OMRPORTLIB->file_write_text) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_write_text is NULL\n");
	}

	if (NULL == OMRPORTLIB->file_get_text_encoding) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_get_text_encoding is NULL\n");
	}

	/* omrfile_test3 */
	if (NULL == OMRPORTLIB->file_vprintf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_vprintf is NULL\n");
	}

	/* omrfile_test17 */
	if (NULL == OMRPORTLIB->file_printf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_printf is NULL\n");
	}

	/* omrfile_test10 */
	if (NULL == OMRPORTLIB->file_seek) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_seek is NULL\n");
	}

	/* omrfile_test11, omrfile_test12 */
	if (NULL == OMRPORTLIB->file_unlink) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_unlink is NULL\n");
	}

	/* omrfile_test4 */
	if (NULL == OMRPORTLIB->file_attr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_attr is NULL\n");
	}

	if (NULL == OMRPORTLIB->file_chmod) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_chmod is NULL\n");
	}

	if (NULL == OMRPORTLIB->file_chown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_chown is NULL\n");
	}

	/* omrfile_test9 */
	if (NULL == OMRPORTLIB->file_lastmod) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_lastmod is NULL\n");
	}

	/* omrfile_test9 */
	if (NULL == OMRPORTLIB->file_length) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_length is NULL\n");
	}

	/* omrfile_test15 */
	if (NULL == OMRPORTLIB->file_sync) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_sync is NULL\n");
	}

	/* omrfile_test25 */
	if (NULL == OMRPORTLIB->file_stat) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_stat is NULL\n");
	}

	if (NULL == OMRPORTLIB->file_stat_filesystem) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_stat_filesystem is NULL\n");
	}

	/* functions  available with standard configuration */
	if (NULL == OMRPORTLIB->file_read_text) { /* TODO omrfiletext.c */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_read_text is NULL\n");
	}

	/* omrfile_test11 */
	if (NULL == OMRPORTLIB->file_mkdir) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_mkdir is NULL\n");
	}

	/* omrfile_test13 */
	if (NULL == OMRPORTLIB->file_move) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_move is NULL\n");
	}

	/* omrfile_test11 */
	if (NULL == OMRPORTLIB->file_unlinkdir) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_unlinkdir is NULL\n");
	}

	/* omrfile_test14 */
	if (NULL == OMRPORTLIB->file_findfirst) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_findfirst is NULL\n");
	}

	/* omrfile_test14 */
	if (NULL == OMRPORTLIB->file_findnext) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_findnext is NULL\n");
	}

	/* omrfile_test14 */
	if (NULL == OMRPORTLIB->file_findclose) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_findclose is NULL\n");
	}

	/* omrfile_test16 */
	if (NULL == OMRPORTLIB->file_error_message) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_error_message is NULL\n");
	}

	if (NULL == OMRPORTLIB->file_convert_native_fd_to_omrfile_fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_convert_native_fd_to_omrfile_fd is NULL\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 * @ref omrfile.c::omrfile_write "omrfile_write()"
 */
TEST_F(PortFileTest2, file_test1)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test1);

	reportTestEntry(OMRPORTLIB, testName);

	/* TODO implement */

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 * @ref omrfile.c::omrfile_write_text "omrfile_write_text()"
 */
TEST_F(PortFileTest2, file_test2)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test2);

	reportTestEntry(OMRPORTLIB, testName);

	/* TODO implement - belongs in omrfiletextTest.c */

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 * @ref omrfile.c::omrfile_vprintf "omrfile_vprintf()"
 */
TEST_F(PortFileTest2, file_test3)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	/* first we need a file to copy use with vprintf */
	const char *testName = APPEND_ASYNC(omrfile_test3);

	int32_t rc;
	const char *file1name = "tfileTest3";
	char expectedResult[] = "Try a number - 1Try a string - abcTry a mixture - 1 abc 2";
	char readBuf[255];
	char *readBufPtr;
	intptr_t fd1;

	reportTestEntry(OMRPORTLIB, testName);

	fd1 = FILE_OPEN_FUNCTION(OMRPORTLIB, file1name, EsOpenCreate | EsOpenWrite, 0666);

	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	fileWrite(OMRPORTLIB, fd1, "Try a number - %d", 1);
	fileWrite(OMRPORTLIB, fd1, "Try a string - %s", "abc");
	fileWrite(OMRPORTLIB, fd1, "Try a mixture - %d %s %d", 1, "abc", 2);

	/* having hopefully shoved this stuff onto file we need to read it back and check its as expected! */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	fd1 = FILE_OPEN_FUNCTION(OMRPORTLIB, file1name, EsOpenRead, 0444);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	readBufPtr = omrfile_read_text(fd1, readBuf, sizeof(readBuf));
	if (NULL == readBufPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read_text() returned NULL\n");
		goto exit;
	}

	if (strlen(readBufPtr) != strlen(expectedResult)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read_text() returned %d expected %d\n", strlen(readBufPtr), strlen(expectedResult));
		goto exit;
	}

	/* omrfile_read_text(): translation from IBM1047 to ASCII is done on zOS */
	if (strcmp(readBufPtr, expectedResult)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read_text() returned \"%s\" expected \"%s\"\n", readBufPtr , expectedResult);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	omrfile_unlink(file1name);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 */
void
openFlagTest(struct OMRPortLibrary *portLibrary, const char *testName, const char *pathName, int32_t flags, int32_t mode)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	intptr_t fd;
	J9FileStat buf;
	int32_t rc;

	/* The file should not exist */
	rc = omrfile_stat(pathName, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, -1);
	}

	/* Open, should succeed */
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, pathName, flags, mode);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
	}
	/* Verify the filesystem sees it */
	rc = omrfile_stat(pathName, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if (buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/* Close the file, should succeed */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}
	/* The file should still exist */
	rc = omrfile_stat(pathName, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if (buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/* Open again, should succeed */
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, pathName, flags, mode);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
	}
	/* The file should still exist */
	rc = omrfile_stat(pathName, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if (buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/* Close the file references, should succeed */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}

	/* Delete this file, should succeed */
	rc = omrfile_unlink(pathName);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() returned %d expected %d\n", rc, -1);
	}
}

/**
 * Verify port file system.
 *
 * Verify @ref omrfile.c::omrfile_attr "omrfile_attr()" correctly
 * determines if files and directories exist.
 */
TEST_F(PortFileTest2, file_test4)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test4);

	const char *localFilename = "tfileTest4.tst";
	char *knownFile = NULL;
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	if (0 == omrsysinfo_get_executable_name("java.properties", &knownFile)) {
		/* NULL name */
		rc = omrfile_attr(NULL);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_attr(NULL) returned %d expected %d\n", rc, -1);
		}

		/* Query a file that should not be there */
		rc = omrfile_attr(localFilename);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_attr(%s) returned %d expected %d\n", localFilename, rc, -1);
		}

		/* Still should not be there */
		rc = omrfile_attr(localFilename);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_attr(%s) returned %d expected %d\n", localFilename, rc, -1);
		}

		/* A directory we know is there, how about the one we are in? */
		rc = omrfile_attr(".");
		if (EsIsDir != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_attr(%s) returned %d expected %d\n", ".", rc, EsIsDir);
		}

		/* Query a file that should be there  */
		rc = omrfile_attr(knownFile);
		if (EsIsFile != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_attr(%s) returned %d expected %d\n", knownFile, rc, EsIsFile);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrsysinfo_get_executable_name(\"java.properties\", &knownFile) failed\n");
	}

	/* Don't delete executable name (knownFile); system-owned. */
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 *
 * Very basic test for open, close, remove functionality.  Does not
 * exhaustively test any of these functions.  It does, however, give an
 * indication if the tests following that exhaustively test operations
 * will pass or fail.
 */
TEST_F(PortFileTest2, file_test5)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test5);

	const char *fileName = "tfileTest5.tst";
	const char *dirName = "tdirTest5";
	intptr_t fd;
	J9FileStat buf;
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	/* There should be no directory or file named above, they are not that usual */
	rc = omrfile_stat(dirName, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}
	rc = omrfile_stat(fileName, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}

	/* try opening a "new" file, expect success. Requires one of read/write attributes too */
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenCreate | EsOpenRead | EsOpenWrite, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s(\"%s\") returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, fileName, -1);
		goto exit;
	}

	/* Close the file, expect success */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}

	/* Create the directory, expect success */
	rc = omrfile_mkdir(dirName);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}

	/* Remove the directory, expect success */
	rc = omrfile_unlinkdir(dirName);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlinkdir(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}

	/* Remove the file, expect success */
	rc = omrfile_unlink(fileName);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() returned %d expected %d\n", rc, 0);
	}

	/* The file and directory should now be gone */
	rc = omrfile_stat(dirName, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}
	rc = omrfile_stat(fileName, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(\"%s\") returned %d expected %d\n", dirName, rc, -1);
	}

	/* try to open a file with no name, should fail */
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, NULL, EsOpenWrite | EsOpenCreate, 0);
	if (-1 != fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}

	/* try to close a non existant file, should fail */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, -1);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, -1);
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 *
 * Open files in the various @ref omrfile.c::PortFileOperations "modes" supported
 * by the port library file operations.  The two functions
 * @ref omrfile.c::omrfile_open "omrfile_open()" and
 * @ref omrfile.c::omrfile_close "omrfile_close()"
 * are verified by this test.
 */
TEST_F(PortFileTest2, file_test6)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test6);

	const char *path1 = "tfileTest6a.tst";
	const char *path2 = "tfileTest6b.tst";
	const char *path3 = "tfileTest6c.tst";
	intptr_t fd1, fd2;
	J9FileStat buf;
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	/* See if the file exists, it shouldn't */
	rc = omrfile_stat(path1, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d expected %d\n", rc, -1);
	}
	/* Create a new file, will fail if the file exists.  Expect success */
	fd1 = FILE_OPEN_FUNCTION(OMRPORTLIB, path1, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}
	/* Verify the filesystem sees it */
	rc = omrfile_attr(path1);
	if (EsIsFile != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_attr() returned %d expected %d\n", rc, EsIsFile);
	}
	/* Try and create this file again, expect failure */
	fd2 = FILE_OPEN_FUNCTION(OMRPORTLIB, path1, EsOpenCreateNew, 0666);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
	/* Verify the filesystem sees it */
	rc = omrfile_stat(path1, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if (buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}
	/* Close the invalid file handle (from second attempt), expect failure */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd2);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, -1);
	}
	/* The file should still exist */
	rc = omrfile_stat(path1, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if (buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}
	/* Close the valid file handle (from first attempt), expect success */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}
	/* The file should still exist */
	rc = omrfile_stat(path1, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, 0);
	} else if (buf.isFile != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isFile = %d, expected %d\n", buf.isFile, 1);
	}

	/*
	 * Testing EsOpenCreateNew Flag - if the file already exist (for which path1 is)
	 * and we specified EsOpenCreateNew, it should failed and
	 * return error number OMRPORT_ERROR_FILE_EXIST
	 */
	fd1 = FILE_OPEN_FUNCTION(OMRPORTLIB, path1, EsOpenCreateNew | EsOpenWrite, 0666);
	if (-1 != fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
		FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	}
/* TODO once all platforms are ready
	if(rc != OMRPORT_ERROR_FILE_EXIST) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned %d expected %d\n", rc, OMRPORT_ERROR_FILE_EXIST);
	}
*/
	/* Finally delete this file */
	rc = omrfile_unlink(path1);
	if (rc < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() returned %d expected %d\n", rc, 0);
	}
	/* The file should not exist */
	rc = omrfile_stat(path1, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, -1);
	}

	/* Series of expected failures, the file does not exist, so can't open it
	 * for read/write truncate etc
	 */
	fd2 = FILE_OPEN_FUNCTION(OMRPORTLIB, path2, EsOpenRead, 0444);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
	fd2 = FILE_OPEN_FUNCTION(OMRPORTLIB, path2, EsOpenWrite, 0666);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
	fd2 = FILE_OPEN_FUNCTION(OMRPORTLIB, path2, EsOpenTruncate, 0666);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
	fd2 = FILE_OPEN_FUNCTION(OMRPORTLIB, path2, EsOpenText, 0666);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
	fd2 = FILE_OPEN_FUNCTION(OMRPORTLIB, path2, EsOpenAppend, 0666);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned valid file handle expected -1\n", FILE_OPEN_FUNCTION_NAME);
	}
/* TODO
 * Different behaviour on windows/UNIX.  Which is right?  Unix states must be one of read/write/execute
 *
	fd2 = omrfile_open(path2, EsOpenCreate, 0444);
	omrfile_close(fd2);
	if (-1 != fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_open() returned valid file handle expected -1\n");
	}
 */

	/* The file should not exist */
	rc = omrfile_stat(path2, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, -1);
	}

	/* flags = EsOpenCreate, requires one of Write/Read */
	openFlagTest(OMRPORTLIB, testName, path3, EsOpenCreate | EsOpenWrite, 0666);

	/* flags = EsOpenWrite */
	openFlagTest(OMRPORTLIB, testName, path3, EsOpenWrite | EsOpenCreate, 0666);

	/* flags = EsOpenTruncate */
/*
	openFlagTest(OMRPORTLIB, testName, path3, EsOpenTruncate | EsOpenCreate);
*/

	/* flags = EsOpenAppend, needs read/write and create */
	openFlagTest(OMRPORTLIB, testName, path3, EsOpenAppend | EsOpenCreate | EsOpenWrite, 0666);

	/* flags = EsOpenText */
	openFlagTest(OMRPORTLIB, testName, path3, EsOpenText | EsOpenCreate | EsOpenWrite, 0666);

	/* flags = EsOpenRead */
	openFlagTest(OMRPORTLIB, testName, path3, EsOpenRead | EsOpenCreate, 0444);

	/* Cleanup, delete this file */
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	omrfile_unlink(path1);
	rc = omrfile_stat(path1, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, -1);
	}

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd2);
	omrfile_unlink(path2);
	rc = omrfile_stat(path2, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, -1);
	}

	rc = omrfile_stat(path3, 0, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d, expected %d\n", rc, -1);
	}

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 *
 * Verify basic
 * @ref omrfile.c::omrfile_read "omrfile_read()" and
 * @ref omrfile::omrfile_write "omrfile_write()"
 * operations
 */
TEST_F(PortFileTest2, file_test7)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test7);

	int32_t expectedReturnValue;
	intptr_t rc;
	const char *fileName = "tfileTest7.tst";
	char inputLine[] = "Input from test7";
	char outputLine[128];
	intptr_t fd;

	reportTestEntry(OMRPORTLIB, testName);

	/* open "new" file - if file exists then should still work */
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenWrite | EsOpenCreate, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* write to it */
	expectedReturnValue = sizeof(inputLine);
	rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd, inputLine, expectedReturnValue);
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc , 0);
		goto exit;
	}

	/* a sneaky test to see that the fd is now useless */
	rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd, inputLine, expectedReturnValue);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenRead, 0444);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* lets read back what we wrote */
	rc = FILE_READ_FUNCTION(OMRPORTLIB, fd, outputLine, 128);
	if (rc != expectedReturnValue)	{
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d, expected %d\n", FILE_READ_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	if (strcmp(outputLine, inputLine)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching written data \"%s\"\n", outputLine, inputLine);
		goto exit;
	}

	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	omrfile_unlink(fileName);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 *
 * Verify
 * @ref omrfile.c::omrfile_read "omrfile_read()" and
 * @ref omrfile::omrfile_write "omrfile_write()"
 * append operations.
 *
 * This test creates a file, writes ABCDE to it then closes it - reopens it for append and
 * writes FGHIJ to it, fileseeks to start of file and reads 10 chars and hopefully this matches
 * ABCDEFGHIJ ....
 */
TEST_F(PortFileTest2, file_test8)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test8);
	intptr_t expectedReturnValue;
	intptr_t rc;
	const char *fileName = "tfileTest8.tst";
	char inputLine1[] = "ABCDE";
	char inputLine2[] = "FGHIJ";
	char outputLine[] = "0123456789";
	char expectedOutput[] = "ABCDEFGHIJ";
	intptr_t fd;
	I_64 filePtr = 0;
	I_64 prevFilePtr = 0;

	reportTestEntry(OMRPORTLIB, testName);
	expectedReturnValue = sizeof(inputLine1) - 1; /* don't want the final 0x00 */

	/*lets try opening a "new" file - if file exists then should still work */
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	prevFilePtr = omrfile_seek(fd, 0, EsSeekCur);
	if (0 != prevFilePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after opening file. Expected = 0, Got = %lld", prevFilePtr);
	}

	/* now lets write to it */
	rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd, inputLine1, expectedReturnValue);

	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	filePtr = omrfile_seek(fd, 0, EsSeekCur);
	if (prevFilePtr + expectedReturnValue != filePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after writing into file. Expected = %lld, Got = %lld", prevFilePtr + expectedReturnValue,  filePtr);
	}

	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenWrite | EsOpenAppend, 0666);

	prevFilePtr = omrfile_seek(fd, 0, EsSeekCur);
#if defined(OMR_OS_WINDOWS)
	/* Windows sets the file pointer to the end of file as soon as it is opened in append mode. */
	if (5 != prevFilePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after opening the file. Expected = 5, Got = %lld", prevFilePtr);
	}
#else
	/* Other platforms set the file pointer to the start of file when it is opened in append mode and update the file pointer to the end only when it is needed in the next call. */
	if (0 != prevFilePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after opening the file. Expected = 0, Got = %lld", prevFilePtr);
	}
#endif /* defined(OMR_OS_WINDOWS) */

	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* now lets write to it */
	expectedReturnValue = sizeof(inputLine2) - 1; /* don't want the final 0x00 */
	rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd, inputLine2, expectedReturnValue);

	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	filePtr = omrfile_seek(fd, 0, EsSeekCur);
	if (10 != filePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after writing into file. Expected = 10, Got = %lld", filePtr);
	}

	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenRead, 0444);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	prevFilePtr = omrfile_seek(fd, 0, EsSeekCur);
	if (0 != prevFilePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after opening the file. Expected = 0, Got = %lld", prevFilePtr);
	}

	/* lets read back what we wrote */
	expectedReturnValue = 10;
	rc = FILE_READ_FUNCTION(OMRPORTLIB, fd, outputLine, expectedReturnValue);
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d, expected %d\n", FILE_READ_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	filePtr = omrfile_seek(fd, 0, EsSeekCur);
	if (prevFilePtr + expectedReturnValue != filePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after reading from file. Expected = %lld, Got = %lld", prevFilePtr + expectedReturnValue, filePtr);
	}

	if (strcmp(expectedOutput, outputLine)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Read back data \"%s\" not matching expected data \"%s\"\n", outputLine, expectedOutput);
		goto exit;
	}

	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	omrfile_unlink(fileName);
	reportTestExit(OMRPORTLIB, testName);
}


/**
 * Verify port file system.
 * @ref omrfile.c::omrfile_lastmod "omrfile_lastmod()"
 * @ref omrfile.c::omrfile_length "omrfile_length()"
 * @ref omrfile.c::omrfile_set_length "omrfile_set_length()"
 */
TEST_F(PortFileTest2, file_test9)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test9);

	const char *fileName = "tfileTest9.tst";
	I_64 lastmod = 0;
	I_64 lastmodPrev = -1;
	intptr_t fd;
	I_64 length, savedLength, newLength;
	intptr_t rc, i;
	intptr_t numberIterations, timesEqual, timesLess;

	reportTestEntry(OMRPORTLIB, testName);

	/* Fast file systems may report time equal or before the previous value
	 * Take a good sampling.  If timeEqual + timesLess != numberIterations then
	 * time does advance, and lastmod is returning changing values
	 */
	numberIterations = 100;
	timesEqual = 0;
	timesLess = 0;
	for (i = 0; i < numberIterations; i++) {
		lastmod = omrfile_lastmod(fileName);

		if (i != 0) {
			if (-1 == lastmod) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_lastmod() returned -1, iteration %d\n", i + 1);
				goto exit;
			}
		} else {
			lastmod = 0;
			lastmodPrev = lastmod;
		}

		fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenCreate | EsOpenWrite | EsOpenAppend, 0666);
		if (-1 == fd) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned -1 expected valid file handle iteration %d\n", FILE_OPEN_FUNCTION_NAME, i + 1);
			goto exit;
		}

		/* make sure we are actually writing something to the file */
		length = omrfile_length(fileName);
		if (length != i) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_length() returned %lld expected %d\n", length, i);
			goto exit;
		}

		if (length > 2) {
			/* Truncate file by 1 */
			savedLength = length;
			newLength = FILE_SET_LENGTH_FUNCTION(OMRPORTLIB, fd, length - 1);
			if (0 != newLength) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %d\n", FILE_SET_LENGTH_FUNCTION_NAME, newLength, 0);
				goto exit;
			}

			/* Verify see correct length */
			length = omrfile_length(fileName);
			if (length != (savedLength - 1)) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_length() returned %lld expected %lld\n", length, savedLength - 1);
				goto exit;
			}

			/* Restore length */
			newLength = FILE_SET_LENGTH_FUNCTION(OMRPORTLIB, fd, savedLength);
			if (0 != newLength) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %d\n", FILE_SET_LENGTH_FUNCTION_NAME, newLength, 0);
				goto exit;
			}
			length = omrfile_length(fileName);
			if (length != savedLength) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_length() returned %lld expected %lld\n", length, savedLength);
				goto exit;
			}
		}

		/* Write something to the file and close it */
		rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd, (char *)"a", 1);
		if (1 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 1, iteration %d\n", FILE_WRITE_FUNCTION_NAME, rc, i + 1);
			goto exit;
		}
		rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d, iteration %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0, i + 1);
			goto exit;
		}

		/* lastmod should have advanced due to the delay */
		lastmod = omrfile_lastmod(fileName);
		if (-1 == lastmod) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_lastmod() returned %d expected %d, iteration %d\n", lastmod, -1, i + 1);
			goto exit;
		}

		/* Time advancing? Account for some slow IO devices */
		if (lastmod == lastmodPrev) {
			timesEqual++;
		} else if (lastmod < lastmodPrev) {
			timesLess++;
		}

		/* do it again */
		lastmodPrev = lastmod;
	}

	/* Was time advancing? */
	if (numberIterations == (timesEqual + timesLess)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_lastmod() failed time did not advance\n");
	}

exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	omrfile_unlink(fileName);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 * @ref omrfile.c::omrfile_seek "omrfile_seek()"
 */
TEST_F(PortFileTest2, file_test10)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test10);

	intptr_t expectedReturnValue;
	intptr_t rc;
	const char *fileName = "tfileTest10.tst";
	char inputLine1[] = "01234567890123456789012345678901234567890123456789012345678901234567890123456789";
	char inputLine2[] = "j9WasHere";
	intptr_t fd;
	I_64 resultingOffset;
	I_64 expectedOffset;
	I_64 offset;
	I_64 fileLength;
	I_64 expectedFileLength = 81;

	reportTestEntry(OMRPORTLIB, testName);

	/* Open new file, will overwrite.  ESOpenTruncate will collapse file
	 */
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}
	omrfile_sync(fd); /* need this to ensure that length matches what we expect */

	/* now lets look at file length - should always be 0 */
	fileLength = omrfile_length(fileName);
	if (0 != fileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_length() returned %lld expected 0\n", fileLength);
		goto exit;
	}

	/* now lets write to it */
	expectedReturnValue = sizeof(inputLine1);
	rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd, inputLine1, expectedReturnValue);
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	/* leave file close and reopen without truncate option (or create) */
	/* close the file */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenWrite, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* lets find current offset */
	offset = 0;
	expectedOffset = 0;
	resultingOffset = omrfile_seek(fd, offset, EsSeekCur);
	if (resultingOffset != expectedOffset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected %lld\n", resultingOffset, expectedOffset);
		goto exit;
	}

	/* lets seek to pos 40 in file */
	offset = 40;
	expectedOffset = 40;
	resultingOffset = omrfile_seek(fd, offset, EsSeekSet);
	if (resultingOffset != expectedOffset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected %lld\n", resultingOffset, expectedOffset);
		goto exit;
	}

	/* ok now lets write something - should overwrite total file length should remain 81 */
	expectedReturnValue = sizeof(inputLine2);
	rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd, inputLine2, expectedReturnValue);
	omrfile_sync(fd); /* need this to ensure that length matches what we expect */
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	/* now lets look at file length */
	fileLength = omrfile_length(fileName);
	if (fileLength != expectedFileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_length() returned %lld expected %lld\n", fileLength, expectedFileLength);
		goto exit;
	}

	/* lets try seeking beyond end of file - this should be allowed however the
	 * length of the file will remain untouched until we do a write
	 */
	resultingOffset = omrfile_seek(fd, fileLength + 5, EsSeekSet);
	if (resultingOffset != fileLength + 5) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected %lld\n", resultingOffset, fileLength + 5);
		goto exit;
	}
	omrfile_sync(fd); /* need this to ensure that length matches what we expect */

	/* now lets look at file length */
	fileLength = omrfile_length(fileName);
	if (fileLength != expectedFileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_length() returned %lld expected %lld\n", fileLength, expectedFileLength);
		goto exit;
	}

	/* Now lets write to the file - expecting some nulls to be inserted and
	 * file length to change appropriately
	 */
	rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd, (char *)"X", 1); /* Note this should just do 1 character X*/
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 1\n", FILE_WRITE_FUNCTION_NAME, rc);
		goto exit;
	}
	omrfile_sync(fd); /* need this to ensure that length matches what we expect */

	/* now lets look at file length - should be 87 */
	expectedFileLength += 6; /* we seeked 5 past end and have written 1 more */
	fileLength = omrfile_length(fileName);
	if (fileLength != expectedFileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_length() returned %lld expected %lld\n", fileLength, expectedFileLength);
		goto exit;
	}

	/* lets seek to pos 40 in file */
	offset = 40;
	expectedOffset = 40;
	resultingOffset = omrfile_seek(fd, offset, EsSeekSet);
	if (resultingOffset != expectedOffset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected %lld\n", resultingOffset, expectedOffset);
		goto exit;
	}

	/* lets try seeking to a -ve offset */
	resultingOffset = omrfile_seek(fd, -3, EsSeekSet);
	if (-1 != resultingOffset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected -1\n", resultingOffset);
		goto exit;
	}

	/* lets see that current offset is still 40 */
	resultingOffset = omrfile_seek(fd, 0, EsSeekCur);
	if (40 != resultingOffset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_seek() returned %lld expected 40\n", resultingOffset);
		goto exit;
	}

	/* close the file */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	omrfile_unlink(fileName);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 *
 * Create directories @ref omrfile.c::omrfile_mkdir "omrfile_mkdir() and
 * delete them @ref omrfile.c::omrfile_unlinkdir "omrfile_unlinkdir()"
 */
TEST_F(PortFileTest2, file_test11)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test11);

	const char *dir2name = "tsubdirTest11";
	char dir1[] = "tdirTest11";
	char dir2[128];
	J9FileStat buf;
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	memset(dir2, 0, sizeof(dir2));
	strcat(dir2, dir1);
	strcat(dir2, DIR_SEPARATOR_STR);
	strcat(dir2, dir2name);

	/* remove the directory.  don't verify if it isn't there then a failure
	 * is returned, if it is there then success is returned
	 */
	omrfile_unlinkdir(dir1);

	/* Create the directory, expect success */
	rc = omrfile_mkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir() returned %d expected %d\n", rc, 0);
	}

	/* Make sure it is there */
	rc = omrfile_stat(dir1, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d expected %d\n", rc, 0);
	} else if (buf.isDir != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isDir = %d, expected %d\n", buf.isDir, 1);
	}

	/* Create the directory again, expect failure */
	rc = omrfile_mkdir(dir1);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir() returned %d expected %d\n", rc, -1);
	}

	/* Remove the directory, expect success */
	rc = omrfile_unlinkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlinkdir() returned %d expected 0\n", rc);
		goto exit;
	}

	/* OK basic test worked
	 * Try putting a file in the directory (and using DIR_SEPARATOR_STR)
	 * first remove the directory in case it is there, don't check for failure/success
	 */
	omrfile_unlinkdir(dir2);

	/* Create the directory, expect failure (assumes all parts of path up to the
	 * last component are there)
	 */
	rc = omrfile_mkdir(dir2);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir() returned %d expected %d\n", rc, -1);
		goto exit;
	}

	/* Create the first part of the directory name, expect success */
	rc = omrfile_mkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/* Create the second part of the name, expect success */
	rc = omrfile_mkdir(dir2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/* Does the file system see it? */
	rc = omrfile_stat(dir2, 0, &buf);
	if (rc != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() returned %d expected %d\n", rc, 0);
	} else if (buf.isDir != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() set buf.isDir = %d, expected %d\n", buf.isDir, 1);
	}

	/* Remove this directory, expect failure (too high) */
	rc = omrfile_unlinkdir(dir1);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlinkdir() returned %d expected %d\n", rc, -1);
		goto exit;
	}

	/* Remove this directory, expect success */
	rc = omrfile_unlinkdir(dir2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlinkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/* Remove this directory, expect success */
	rc = omrfile_unlinkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlinkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

exit:
	omrfile_unlinkdir(dir2);
	omrfile_unlinkdir(dir1);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify file
 * @ref omrfile.c::omrfile_unlink "omrfile_unlink()" and
 * @ref omrfile.c::omrfile_unlinkdir "omrfile_unlinkdir()"
 */
TEST_F(PortFileTest2, file_test12)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test12);

	int32_t rc;
	intptr_t fd1 = -1;
	intptr_t fd2 = -1;
	char dir1[] = "tdirTest12";
	char pathName1[128];
	char pathName2[128];
	const char *file1name = "tfileTest12a.tst";
	const char *file2name = "tfileTest12b.tst";

	reportTestEntry(OMRPORTLIB, testName);

	memset(pathName1, 0, sizeof(pathName1));
	strcat(pathName1, dir1);
	strcat(pathName1, DIR_SEPARATOR_STR);
	strcat(pathName1, file1name);

	memset(pathName2, 0, sizeof(pathName2));
	strcat(pathName2, dir1);
	strcat(pathName2, DIR_SEPARATOR_STR);
	strcat(pathName2, file2name);

	/* create a directory */
	rc = omrfile_mkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir() returned %d expected 0\n", rc);
		goto exit;
	}

	/* create some files within directory */
	fd1 = FILE_OPEN_FUNCTION(OMRPORTLIB, pathName1, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	fd2 = FILE_OPEN_FUNCTION(OMRPORTLIB, pathName2, EsOpenCreate | EsOpenWrite , 0666);
	if (-1 == fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	/* try unlinking a non-existant file */
	rc = omrfile_unlink("rubbish");
	if (-1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() returned %d expected -1\n", rc);
		goto exit;
	}

	/* Unix allows unlink of open files, windows doesn't */

	/* try unlink of closed file */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

	rc = omrfile_unlink(pathName1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() returned %d expected 0\n", rc);
		goto exit;
	}

	/* try open of file just unlinked: Don't use helper as expect failure */
	fd1 = FILE_OPEN_FUNCTION(OMRPORTLIB, pathName1, EsOpenWrite , 0666);
	if (-1 != fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	/* try unlinkdir of non-existant directory */
	rc = omrfile_unlinkdir("rubbish");
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlinkdir() returned %d expected -1\n", rc);
		goto exit;
	}

	/* try unlinkdir on directory with files in it */
	rc = omrfile_unlinkdir(dir1);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlinkdir() returned %d expected -1\n", rc);
		goto exit;
	}

	/* delete file from dir and retry unlinkdir */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 0\n", FILE_CLOSE_FUNCTION_NAME, rc);
		goto exit;
	}

	rc = omrfile_unlink(pathName2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() returned %d expected 0\n", rc);
		goto exit;
	}

	rc = omrfile_unlinkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlinkdir() returned %d expected 0\n", rc);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd2);
	omrfile_unlink(pathName1);
	omrfile_unlink(pathName2);
	omrfile_unlinkdir(dir1);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify file move operation
 * @ref omrfile.c::omrfile_move "omrfile_move()"
 */
TEST_F(PortFileTest2, file_test13)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	/* This case tests ability to create a directory/file and rename it */
	const char *testName = APPEND_ASYNC(omrfile_test13);

	int32_t rc;
	intptr_t fd1 = -1;
	char dir1[] = "tdirTest13a";
	char dir2[] = "tdirTest13b";
	char pathName1[128];
	char pathName2[128];
	const char *file1name = "tfileTest13a.tst";
	const char *file2name = "tfileTest13b.tst";

	reportTestEntry(OMRPORTLIB, testName);

	memset(pathName1, 0, sizeof(pathName1));
	strcat(pathName1, dir2);
	strcat(pathName1, DIR_SEPARATOR_STR);
	strcat(pathName1, file1name);

	memset(pathName2, 0, sizeof(pathName2));
	strcat(pathName2, dir2);
	strcat(pathName2, DIR_SEPARATOR_STR);
	strcat(pathName2, file2name);

	/* create a directory */
	rc = omrfile_mkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/* now rename it */
	rc = omrfile_move(dir1, dir2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_move() from %s to %s returned %d, expected %d\n", dir1, dir2, rc, 0);
		goto exit;
	}

	/* create a file within the directory */
	fd1 = FILE_OPEN_FUNCTION(OMRPORTLIB, pathName1, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* so close it and try again */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1); /* should have 0 length file */
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	rc = omrfile_move(pathName1, pathName2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_move() from %s to %s returned %d, expected %d\n", pathName1, pathName2, rc, 0);
		goto exit;
	}

	/* could add in some stuff to check that file exists..... */

exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	omrfile_unlink(pathName1);
	omrfile_unlink(pathName2);
	omrfile_unlinkdir(dir1);
	omrfile_unlinkdir(dir2);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 * @ref omrfile.c::omrfile_findfirst "omrfile_findfirst()"
 * @ref omrfile.c::omrfile_findnext "omrfile_findnext()"
 * @ref omrfile.c::omrfile_findclose "omrfile_findclose()"
 */
TEST_F(PortFileTest2, file_test14)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test14);

	intptr_t rc;
	intptr_t expectedReturnValue;
	intptr_t fd1 = -1;
	intptr_t fd2 = -1;
	uintptr_t handle;

	const char *dir2name = "tdirTest14a";
	char dir1[] = "tdirTest14b";
	char dir2[128];
	char resultbuffer[128];
	char pathName1[128];
	char pathName2[128];
	const char *file1name = "tfileTest14a.tst";
	const char *file2name = "tfileTest14b.tst";
	char search[128];
	/*int32_t count=0;
	int32_t expectedCount=3;*/ /* dont forget that the tdirTest14a inside tdirTest14b also counts for the search */
	/*int32_t fileCount=0;
	int32_t expectedFileCount=2;
	int32_t dirCount=0;
	int32_t expectedDirCount=1;
	char fullname[128]="";*/

	reportTestEntry(OMRPORTLIB, testName);
	memset(dir2, 0, sizeof(dir2));
	strcat(dir2, dir1);
	strcat(dir2, DIR_SEPARATOR_STR);
	strcat(dir2, dir2name);

	memset(pathName1, 0, sizeof(pathName1));
	strcat(pathName1, dir1);
	strcat(pathName1, DIR_SEPARATOR_STR);
	strcat(pathName1, file1name);

	memset(pathName2, 0, sizeof(pathName2));
	strcat(pathName2, dir1);
	strcat(pathName2, DIR_SEPARATOR_STR);
	strcat(pathName2, file2name);

	memset(search, 0, sizeof(search));
	strcat(search, dir1);
	strcat(search, DIR_SEPARATOR_STR);
	strcat(search, "t*Test14*");

	rc = omrfile_mkdir(dir1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	rc = omrfile_mkdir(dir2);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir() returned %d expected %d\n", rc, 0);
		goto exit;
	}

	/* try opening a "new" file - if file exists then should still work */
	fd1 = FILE_OPEN_FUNCTION(OMRPORTLIB, pathName1, EsOpenWrite | EsOpenCreate, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* try opening a "new" file - if file exists then should still work */
	fd2 = FILE_OPEN_FUNCTION(OMRPORTLIB, pathName2, EsOpenWrite | EsOpenCreate, 0666);
	if (-1 == fd2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected valid file handle\n", FILE_OPEN_FUNCTION_NAME, -1);
		goto exit;
	}

	/* write to them */
	expectedReturnValue = sizeof("test");
	rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd1, (char *)"test", expectedReturnValue);
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd2, (char *)"test", expectedReturnValue);
	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	/* close this one */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	/* Now we can test findfirst etc..... */
	handle = omrfile_findfirst("gobbledygook*", resultbuffer);
	if ((uintptr_t) - 1 != handle) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_findfirst() returned valid handle expected -1\n");
		goto exit;
	}

/*
 * TODO: different behaviour on windows/Linux
 *
	handle = omrfile_findfirst(search, resultbuffer);
	if (-1 == handle) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_findfirst() returned -1 expected a valid handle\n");
		goto exit;
	}

	memset(fullname,0,sizeof(fullname));
	strcat(fullname,dir1);
	strcat(fullname,DIR_SEPARATOR_STR);
	strcat(fullname,resultbuffer);
	attrRc = omrfile_attr(fullname);
	if (EsIsFile == attrRc) {
		fileCount++;
	}
	if (EsIsDir == attrRc) {
		dirCount++;
	}

	rc = 0;
	while (rc != -1) {
		count ++;
		rc = omrfile_findnext(handle, resultbuffer);
		if (rc >= 0) {
			memset(fullname,0,sizeof(fullname));
			strcat(fullname,dir1);
			strcat(fullname,DIR_SEPARATOR_STR);
			strcat(fullname,resultbuffer);
			attrRc = omrfile_attr(fullname);

			if (EsIsFile == attrRc) {
				fileCount++;
			}
			if (EsIsDir == attrRc) {
				dirCount++;
			}
		}
	}

	if (fileCount != expectedFileCount)	{
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file count incorrrect, found %d expected %d\n", fileCount, expectedFileCount);
		goto exit;
	}

	if (dirCount != expectedDirCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "directory count incorrect, found %d expected %d\n", dirCount, expectedDirCount);
		goto exit;
	}

	if (count != expectedCount) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "count incorrect, found %d expected %d\n", count, expectedCount);
		goto exit;
	}
	omrfile_findclose(handle);
 *
 */

exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd2);
	omrfile_unlink(pathName1);
	omrfile_unlink(pathName2);
	omrfile_unlinkdir(dir2);
	omrfile_unlinkdir(dir1);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 * @ref omrfile.c::omrfile_sync "omrfile_sync()"
 */
TEST_F(PortFileTest2, file_test15)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test15);

	const char *fileName = "tfileTest15.tst";
	I_64 lastmod = 0;
	I_64 lastmodPrev = -1;
	intptr_t fd;
	I_64 length;
	intptr_t rc, i;
	intptr_t numberIterations, timesEqual, timesLess;

	reportTestEntry(OMRPORTLIB, testName);

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenCreate | EsOpenWrite | EsOpenAppend, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned -1 expected valid file handle\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	/* Fast file systems may report time equal or before the previous value
	 * Take a good sampling.  If timeEqual + timesLess != numberIterations then
	 * time does advance, and lastmod is returning changing values
	 */
	numberIterations = 100;
	timesEqual = 0;
	timesLess = 0;
	for (i = 0; i < numberIterations; i++) {
		lastmod = omrfile_lastmod(fileName);
		if (0 != i) {
			if (-1 == lastmod) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_lastmod() returned -1, iteration %d\n", i + 1);
				goto exit;
			}
		} else {
			lastmod = 0;
			lastmodPrev = lastmod;
		}

		/* make sure we are actually writing something to the file */
		length = omrfile_length(fileName);
		if (length != i) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_length() returned %lld expected %d\n", length, i);
			goto exit;
		}

		/* Write something to the file and close it */
		rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd, (char *)"a", 1);
		if (1 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected 1, iteration %d\n", FILE_WRITE_FUNCTION_NAME, rc, i + 1);
			goto exit;
		}

		rc = omrfile_sync(fd);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_sync() returned %d expected %d, iteration %d\n", rc, 0, i + 1);
			goto exit;
		}

		/* lastmod should have advanced due to the delay */
		lastmod = omrfile_lastmod(fileName);
		if (-1 == lastmod) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_lastmod() returned %d expected %d, iteration %d\n", lastmod, -1, i + 1);
			goto exit;
		}

		/* Time advancing? Account for some slow IO devices */
		if (lastmod == lastmodPrev) {
			timesEqual++;
		} else if (lastmod < lastmodPrev) {
			timesLess++;
		}

		/* do it again */
		lastmodPrev = lastmod;
	}

	/* Was time always the same? */
	if (numberIterations == (timesEqual + timesLess)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_lastmod() failed time did not advance\n");
	}

exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	omrfile_unlink(fileName);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 * Verify @ref omrfile.c::omrfile_error_message "omrfile_error_message()".
 */
TEST_F(PortFileTest2, file_test16)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test16);

	reportTestEntry(OMRPORTLIB, testName);

	/* TODO implement */

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify file printf function.
 * Verify @ref omrfile.c::omrfile_printf "omrfile_printf()"
 */
TEST_F(PortFileTest2, file_test17)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	/* first we need a file to copy use with vprintf */
	const char *testName = APPEND_ASYNC(omrfile_test17);

	int32_t rc;
	const char *file1name = "tfileTest17";
	char expectedResult[] = "Try a number - 1Try a string - abcTry a mixture - 1 abc 2";
	char readBuf[255];
	char *readBufPtr;
	intptr_t fd1;

	reportTestEntry(OMRPORTLIB, testName);

	fd1 = FILE_OPEN_FUNCTION(OMRPORTLIB, file1name, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	omrfile_printf(fd1, "Try a number - %d", 1);
	omrfile_printf(fd1, "Try a string - %s", "abc");
	omrfile_printf(fd1, "Try a mixture - %d %s %d", 1, "abc", 2);

	/* having hopefully shoved this stuff onto file we need to read it back and check its as expected! */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	fd1 = FILE_OPEN_FUNCTION(OMRPORTLIB, file1name, EsOpenRead, 0444);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	readBufPtr = omrfile_read_text(fd1, readBuf, sizeof(readBuf));
	if (NULL == readBufPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read_text() returned NULL\n");
		goto exit;
	}

	if (strlen(readBufPtr) != strlen(expectedResult)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read_text() returned %d expected %d\n", strlen(readBufPtr), strlen(expectedResult));
		goto exit;
	}

	/* omrfile_read_text(): translation from IBM1047 to ASCII is done on zOS */
	if (strcmp(readBufPtr, expectedResult)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read_text() returned \"%s\" expected \"%s\"\n", readBufPtr , expectedResult);
		goto exit;
	}

exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
	omrfile_unlink(file1name);
	reportTestExit(OMRPORTLIB, testName);
}

/*
 * Helper function used by the File Locking tests below to create the base file
 * used by the tests
 *
 */
intptr_t
omrfile_create_file(OMRPortLibrary *portLibrary, const char *filename, int32_t openMode, const char *testName)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t returnFd = -1;
	intptr_t createFd = -1;
	char buffer[100];
	intptr_t i;
	intptr_t rc;

	for (i = 0; i < 100; i++) {
		buffer[i] = (char)i;
	}

	omrfile_unlink(filename);

	createFd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == createFd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		return returnFd;
	}

	rc = omrfile_write(createFd, buffer, 100);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Write to file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		omrfile_close(createFd);
		return returnFd;
	}

	rc = omrfile_close(createFd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		return returnFd;
	}

	if (0 == openMode) {
		returnFd = 0;
	} else {
		returnFd = omrfile_open(filename, openMode, 0);
		if (-1 == returnFd) {
			lastErrorMessage = (char *)omrerror_last_error_message();
			lastErrorNumber = omrerror_last_error_number();
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Open of file %s in mode %d failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, openMode, lastErrorNumber, lastErrorMessage);
			return returnFd;
		}
	}

	return returnFd;
}

/*
 * Helper function used by File Locking tests below to create a file indicating
 * that the test has reached a certain stage in its processing
 *
 * Status files are used as we do not have a cross platform inter process
 * semaphore facility on all platforms
 *
 */
intptr_t
omrfile_create_status_file(OMRPortLibrary *portLibrary, const char *filename, const char *testName)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t createFd = -1;
	int rc;

	omrfile_unlink(filename);

	createFd = omrfile_open(filename, EsOpenCreateNew | EsOpenRead | EsOpenWrite, 0660);
	if (-1 == createFd) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Create of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		return -1;
	}

	rc = omrfile_close(createFd);
	if (-1 == rc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Close of file %s failed: lastErrorNumber=%d, lastErrorMessage=%s\n", filename, lastErrorNumber, lastErrorMessage);
		return -1;
	}

	return 0;
}

/*
 * Helper function used by File Locking tests below to check for the existance
 * of a status file from the other child in the test
 */
intptr_t
omrfile_status_file_exists(OMRPortLibrary *portLibrary, const char *filename, const char *testName)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	int32_t rc;
	J9FileStat buf;

	rc = omrfile_stat(filename, 0, &buf);
	if ((rc == 0) && (buf.isFile == 1)) {
		return 1;
	}

	return 0;
}

/*
 * File Locking Tests
 *
 * Basic test: ensure we can acquire and release a read lock
 *
 */
TEST_F(PortFileTest2, file_test18)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	const char *testName = APPEND_ASYNC(omrfile_test18);
	const char *testFile = APPEND_ASYNC(omrfile_test18_file);
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = 16;
	uint64_t length = 4;

	reportTestEntry(OMRPORTLIB, testName);

	fd = omrfile_create_file(OMRPORTLIB, testFile, EsOpenRead, testName);
	if (-1 == fd) {
		/* No need to print error messages, omrfile_create_file will already have done so */
		goto exit;
	}

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_READ_LOCK | OMRPORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	omrfile_unlink(testFile);
exit:

	reportTestExit(OMRPORTLIB, testName);
}

/*
 * File Locking Tests
 *
 * Basic test: ensure we can acquire and release a write lock
 *
 */
TEST_F(PortFileTest2, file_test19)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	const char *testName = APPEND_ASYNC(omrfile_test19);
	const char *testFile = APPEND_ASYNC(omrfile_test19_file);
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = 16;
	uint64_t length = 4;

	reportTestEntry(OMRPORTLIB, testName);

	fd = omrfile_create_file(OMRPORTLIB, testFile, EsOpenWrite, testName);
	if (-1 == fd) {
		/* No need to print error messages, omrfile_create_file will already have done so */
		goto exit;
	}

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	omrfile_unlink(testFile);

exit:

	reportTestExit(OMRPORTLIB, testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets write lock
 *    Child 2 tries to get write lock on same bytes, fails
 *
 */
#define TEST20_OFFSET 16
#define TEST20_LENGTH 4
TEST_F(PortFileTest2, file_test20)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRProcessHandle pid1 = NULL;
	OMRProcessHandle pid2 = NULL;
	intptr_t termstat1;
	intptr_t termstat2;
	intptr_t rc;
	const char *testName = APPEND_ASYNC(omrfile_test20);
	const char *testFile = "omrfile_test20_file";
	const char *testFileLock = "omrfile_test20_child_1_got_lock";
	const char *testFileDone = "omrfile_test20_child_2_done";
	char *argv0 = portTestEnv->_argv[0];

	reportTestEntry(OMRPORTLIB, testName);

	/* Remove status files */
	omrfile_unlink(testFileLock);
	omrfile_unlink(testFileDone);

	rc = omrfile_create_file(OMRPORTLIB, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, omrfile_create_file will already have done so */
		goto exit;
	}

	/* parent */

	pid1 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test20_1");
	if (NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test20_2");
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
omrfile_test20_child_1(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const char *testName = APPEND_ASYNC(omrfile_test20_child_1);
	const char *testFile = "omrfile_test20_file";
	const char *testFileLock = "omrfile_test20_child_1_got_lock";
	const char *testFileDone = "omrfile_test20_child_2_done";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = TEST20_OFFSET;
	uint64_t length = TEST20_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	omrfile_create_status_file(OMRPORTLIB, testFileLock, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileDone, testName)) {
		SleepFor(1);
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

int
omrfile_test20_child_2(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const char *testName = APPEND_ASYNC(omrfile_test20_child_2);
	const char *testFile = "omrfile_test20_file";
	const char *testFileLock = "omrfile_test20_child_1_got_lock";
	const char *testFileDone = "omrfile_test20_child_2_done";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	uint64_t offset = TEST20_OFFSET;
	uint64_t length = TEST20_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileLock, testName)) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (-1 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld incorrectly succeeded: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

	omrfile_create_status_file(OMRPORTLIB, testFileDone, testName);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets write lock
 *    Child 2 tries to get write lock on adjacent bytes, succeeds
 *
 */
#define TEST21_OFFSET 16
#define TEST21_LENGTH 4
TEST_F(PortFileTest2, file_test21)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRProcessHandle pid1 = NULL;
	OMRProcessHandle pid2 = NULL;
	intptr_t termstat1;
	intptr_t termstat2;
	intptr_t rc;
	const char *testName = APPEND_ASYNC(omrfile_test21);
	const char *testFile = "omrfile_test21_file";
	const char *testFileLock = "omrfile_test21_child_1_got_lock";
	const char *testFileDone = "omrfile_test21_child_2_done";
	char *argv0 = portTestEnv->_argv[0];

	reportTestEntry(OMRPORTLIB, testName);

	/* Remove status files */
	omrfile_unlink(testFileLock);
	omrfile_unlink(testFileDone);

	rc = omrfile_create_file(OMRPORTLIB, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, omrfile_create_file will already have done so */
		goto exit;
	}

	pid1 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test21_1");
	if (NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test21_2");
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

	/* Remove status files */
	omrfile_unlink(testFile);
	omrfile_unlink(testFileLock);
	omrfile_unlink(testFileDone);

exit:

	reportTestExit(OMRPORTLIB, testName);
}

int
omrfile_test21_child_1(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char *testName = APPEND_ASYNC(omrfile_test21_child_1);
	const char *testFile = "omrfile_test21_file";
	const char *testFileLock = "omrfile_test21_child_1_got_lock";
	const char *testFileDone = "omrfile_test21_child_2_done";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = TEST21_OFFSET;
	uint64_t length = TEST21_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	omrfile_create_status_file(OMRPORTLIB, testFileLock, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileDone, testName)) {
		SleepFor(1);
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

int
omrfile_test21_child_2(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const char *testName = APPEND_ASYNC(omrfile_test21_child_2);
	const char *testFile = "omrfile_test21_file";
	const char *testFileDone = "omrfile_test21_child_2_done";
	const char *testFileLock = "omrfile_test21_child_1_got_lock";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = TEST21_OFFSET + TEST21_LENGTH;
	uint64_t length = TEST21_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileLock, testName)) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

	omrfile_create_status_file(OMRPORTLIB, testFileDone, testName);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets write lock
 *    Child 2 tries to get write lock on same bytes, waits
 *    Child 1 releases lock
 *    Child 2 acquires lock
 *
 */
#define TEST22_OFFSET 16
#define TEST22_LENGTH 4
TEST_F(PortFileTest2, file_test22)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRProcessHandle pid1 = NULL;
	OMRProcessHandle pid2 = NULL;
	intptr_t termstat1;
	intptr_t termstat2;
	intptr_t rc;
	const char *testName = APPEND_ASYNC(omrfile_test22);
	const char *testFile = "omrfile_test22_file";
	const char *testFileDone = "omrfile_test22_child_2_done";
	const char *testFileLock = "omrfile_test22_child_1_got_lock";
	const char *testFileTry = "omrfile_test22_child_2_trying_for_lock";
	char *argv0 = portTestEnv->_argv[0];

	reportTestEntry(OMRPORTLIB, testName);

	/* Remove status files */
	omrfile_unlink(testFileLock);
	omrfile_unlink(testFileTry);
	omrfile_unlink(testFileDone);

	rc = omrfile_create_file(OMRPORTLIB, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, omrfile_create_file will already have done so */
		goto exit;
	}

	pid1 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test22_1");
	if (NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test22_2");
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
omrfile_test22_child_1(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	const char *testName = APPEND_ASYNC(omrfile_test22_child_1);
	const char *testFile = "omrfile_test22_file";
	const char *testFileLock = "omrfile_test22_child_1_got_lock";
	const char *testFileTry = "omrfile_test22_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = TEST22_OFFSET;
	uint64_t length = TEST22_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	omrfile_create_status_file(OMRPORTLIB, testFileLock, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileTry, testName)) {
		SleepFor(1);
	}

	SleepFor(1);

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

int
omrfile_test22_child_2(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const char *testName = APPEND_ASYNC(omrfile_test22_child_2);
	const char *testFile = "omrfile_test22_file";
	const char *testFileLock = "omrfile_test22_child_1_got_lock";
	const char *testFileDone = "omrfile_test22_child_2_done";
	const char *testFileTry = "omrfile_test22_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = TEST22_OFFSET;
	uint64_t length = TEST22_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileLock, testName)) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenWrite, 0);

	omrfile_create_status_file(OMRPORTLIB, testFileTry, testName);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_WAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

	omrfile_create_status_file(OMRPORTLIB, testFileDone, testName);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets read lock
 *    Child 2 tries to get read lock on same bytes, succeeds
 *    Child 1 releases lock
 *    Child 2 acquires lock
 *
 */
#define TEST23_OFFSET 16
#define TEST23_LENGTH 4
TEST_F(PortFileTest2, file_test23)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRProcessHandle pid1 = NULL;
	OMRProcessHandle pid2 = NULL;
	intptr_t termstat1;
	intptr_t termstat2;
	intptr_t rc;
	const char *testName = APPEND_ASYNC(omrfile_test23);
	const char *testFile = "omrfile_test23_file";
	const char *testFileDone = "omrfile_test23_child_2_done";
	const char *testFileLock1 = "omrfile_test23_child_1_got_lock";
	const char *testFileLock2 = "omrfile_test23_child_2_got_lock";
	char *argv0 = portTestEnv->_argv[0];

	reportTestEntry(OMRPORTLIB, testName);

	/* Remove status files */
	omrfile_unlink(testFileLock1);
	omrfile_unlink(testFileLock2);
	omrfile_unlink(testFileDone);

	rc = omrfile_create_file(OMRPORTLIB, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, omrfile_create_file will already have done so */
		goto exit;
	}

	pid1 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test23_1");
	if (NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test23_2");
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

	/* Remove status files */
	omrfile_unlink(testFile);
	omrfile_unlink(testFileLock1);
	omrfile_unlink(testFileLock2);
	omrfile_unlink(testFileDone);
exit:

	reportTestExit(OMRPORTLIB, testName);
}

int
omrfile_test23_child_1(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const char *testName = APPEND_ASYNC(omrfile_test23_child_1);
	const char *testFile = "omrfile_test23_file";
	const char *testFileLock1 = "omrfile_test23_child_1_got_lock";
	const char *testFileLock2 = "omrfile_test23_child_2_got_lock";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = TEST23_OFFSET;
	uint64_t length = TEST23_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenRead | EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_READ_LOCK | OMRPORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	omrfile_create_status_file(OMRPORTLIB, testFileLock1, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileLock2, testName)) {
		SleepFor(1);
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

int
omrfile_test23_child_2(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const char *testName = APPEND_ASYNC(omrfile_test23_child_2);
	const char *testFile = "omrfile_test23_file";
	const char *testFileDone = "omrfile_test23_child_2_done";
	const char *testFileLock1 = "omrfile_test23_child_1_got_lock";
	const char *testFileLock2 = "omrfile_test23_child_2_got_lock";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = TEST23_OFFSET;
	uint64_t length = TEST23_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileLock1, testName)) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenRead | EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_READ_LOCK | OMRPORT_FILE_WAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	omrfile_create_status_file(OMRPORTLIB, testFileLock2, testName);

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

	omrfile_create_status_file(OMRPORTLIB, testFileDone, testName);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets read lock
 *    Child 2 tries to get write lock on same bytes, waits
 *    Child 1 releases lock
 *    Child 2 acquires lock
 *
 */
#define TEST24_OFFSET 16
#define TEST24_LENGTH 4
TEST_F(PortFileTest2, file_test24)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRProcessHandle pid1 = 0;
	OMRProcessHandle pid2 = 0;
	intptr_t termstat1;
	intptr_t termstat2;
	intptr_t rc;
	const char *testName = APPEND_ASYNC(omrfile_test24);
	const char *testFile = "omrfile_test24_file";
	const char *testFileDone = "omrfile_test24_child_2_done";
	const char *testFileLock1 = "omrfile_test24_child_1_got_lock";
	const char *testFileTry = "omrfile_test24_child_2_trying_for_lock";
	char *argv0 = portTestEnv->_argv[0];

	reportTestEntry(OMRPORTLIB, testName);

	/* Remove status files */
	omrfile_unlink(testFileLock1);
	omrfile_unlink(testFileTry);
	omrfile_unlink(testFileDone);

	rc = omrfile_create_file(OMRPORTLIB, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, omrfile_create_file will already have done so */
		goto exit;
	}

	pid1 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test24_1");
	if (NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test24_2");
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
omrfile_test24_child_1(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const char *testName = APPEND_ASYNC(omrfile_test24_child_1);
	const char *testFile = "omrfile_test24_file";
	const char *testFileLock1 = "omrfile_test24_child_1_got_lock";
	const char *testFileTry = "omrfile_test24_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = TEST24_OFFSET;
	uint64_t length = TEST24_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenRead | EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_READ_LOCK | OMRPORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	omrfile_create_status_file(OMRPORTLIB, testFileLock1, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileTry, testName)) {
		SleepFor(1);
	}

	SleepFor(1);

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

int
omrfile_test24_child_2(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const char *testName = APPEND_ASYNC(omrfile_test24_child_2);
	const char *testFile = "omrfile_test24_file";
	const char *testFileDone = "omrfile_test24_child_2_done";
	const char *testFileLock1 = "omrfile_test24_child_1_got_lock";
	const char *testFileTry = "omrfile_test24_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = TEST24_OFFSET;
	uint64_t length = TEST24_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileLock1, testName)) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenRead | EsOpenWrite, 0);

	omrfile_create_status_file(OMRPORTLIB, testFileTry, testName);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_WAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

	omrfile_create_status_file(OMRPORTLIB, testFileDone, testName);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

/*
 * File Locking Tests
 *
 * Test:
 *    Child 1 gets read lock
 *    Child 2 tries to get write lock on same bytes, waits
 *    Child 1 changes part of read lock to write lock
 *    Child 1 releases read lock
 *    Child 1 releases write lock
 *    Child 2 acquires lock
 *    Child 2 releases lock
 *
 * Note:
 *    This test does not really add value to the Windows
 *    platforms as it is not possible to release part of
 *    a lock or obtain a write lock on an area that is
 *    partially read locked, so on Windows the initial
 *    read lock is released - the test works, but is
 *    not really different to earlier tests.
 *
 */
#define TEST25_OFFSET 16
#define TEST25_LENGTH 4
TEST_F(PortFileTest2, file_test25)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	OMRProcessHandle pid1 = 0;
	OMRProcessHandle pid2 = 0;
	intptr_t termstat1;
	intptr_t termstat2;
	intptr_t rc;
	const char *testName = APPEND_ASYNC(omrfile_test25);
	const char *testFile = "omrfile_test25_file";
	const char *testFileLock = "omrfile_test25_child_1_got_lock";
	const char *testFileTry = "omrfile_test25_child_2_trying_for_lock";
	const char *testFileDone = "omrfile_test25_child_2_done";
	char *argv0 = portTestEnv->_argv[0];

	reportTestEntry(OMRPORTLIB, testName);

	/* Remove status files */
	omrfile_unlink(testFileLock);
	omrfile_unlink(testFileTry);
	omrfile_unlink(testFileDone);

	rc = omrfile_create_file(OMRPORTLIB, testFile, 0, testName);
	if (0 != rc) {
		/* No need to print error messages, omrfile_create_file will already have done so */
		goto exit;
	}

	pid1 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test25_1");
	if (NULL == pid1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "launchChildProcess() failed for pid1!\n");
		goto exit;
	}
	pid2 = launchChildProcess(OMRPORTLIB, testName, argv0, "-child_omrfile_test25_2");
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
omrfile_test25_child_1(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const char *testName = APPEND_ASYNC(omrfile_test25_child_1);
	const char *testFile = "omrfile_test25_file";
	const char *testFileLock = "omrfile_test25_child_1_got_lock";
	const char *testFileTry = "omrfile_test25_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = TEST25_OFFSET;
	uint64_t length = TEST25_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);
	portTestEnv->changeIndent(1);

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenRead | EsOpenWrite, 0);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_READ_LOCK | OMRPORT_FILE_NOWAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	portTestEnv->log("Child 1: Got read lock on bytes %lld for %lld\n", offset, length);

	omrfile_create_status_file(OMRPORTLIB, testFileLock, testName);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileTry, testName)) {
		SleepFor(1);
	}

	SleepFor(1);

#if defined(OMR_OS_WINDOWS) || defined(OSX)
	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", (offset + 2), (length - 2), lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	portTestEnv->log("Child 1: Released read lock on bytes %lld for %lld\n", offset, length);
#endif /* defined(OMR_OS_WINDOWS) || defined(OSX) */


	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_WAIT_FOR_LOCK, (offset + 2), length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", (offset + 2), length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	portTestEnv->log("Child 1: Got write lock on bytes %lld for %lld\n", (offset + 2), length);

#if !defined(OMR_OS_WINDOWS)
	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, (length - 2));

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, (length - 2), lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	portTestEnv->log("Child 1: Released read lock on bytes %lld for %lld\n", offset, (length - 2));
#endif /* !defined(OMR_OS_WINDOWS) */

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, (offset + 2), length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", (offset + 2), length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	portTestEnv->log("Child 1: Released write lock on bytes %lld for %lld\n", (offset + 2), length);

	portTestEnv->changeIndent(-1);
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

int
omrfile_test25_child_2(OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const char *testName = APPEND_ASYNC(omrfile_test25_child_2);
	const char *testFile = "omrfile_test25_file";
	const char *testFileLock = "omrfile_test25_child_1_got_lock";
	const char *testFileDone = "omrfile_test25_child_2_done";
	const char *testFileTry = "omrfile_test25_child_2_trying_for_lock";
	char *lastErrorMessage = NULL;
	int32_t lastErrorNumber = 0;
	intptr_t fd;
	int32_t lockRc;
	int32_t unlockRc;
	uint64_t offset = TEST25_OFFSET;
	uint64_t length = TEST25_LENGTH;

	reportTestEntry(OMRPORTLIB, testName);
	portTestEnv->changeIndent(1);

	while (!omrfile_status_file_exists(OMRPORTLIB, testFileLock, testName)) {
		SleepFor(1);
	}

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, testFile, EsOpenRead | EsOpenWrite, 0);

	portTestEnv->log("Child 2: Trying for write lock on bytes %lld for %lld\n", offset, length);
	omrfile_create_status_file(OMRPORTLIB, testFileTry, testName);

	lockRc = FILE_LOCK_BYTES_FUNCTION(OMRPORTLIB, fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_WAIT_FOR_LOCK, offset, length);

	if (0 != lockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Locking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	portTestEnv->log("Child 2: Got write lock on bytes %lld for %lld\n", offset, length);

	unlockRc = FILE_UNLOCK_BYTES_FUNCTION(OMRPORTLIB, fd, offset, length);

	if (0 != unlockRc) {
		lastErrorMessage = (char *)omrerror_last_error_message();
		lastErrorNumber = omrerror_last_error_number();
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unlocking of bytes %lld for %lld failed: lastErrorNumber=%d, lastErrorMessage=%s\n", offset, length, lastErrorNumber, lastErrorMessage);
		goto exit;
	}
	portTestEnv->log("Child 2: Released write lock on bytes %lld for %lld\n", offset, length);

	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	portTestEnv->changeIndent(-1);
	omrfile_create_status_file(OMRPORTLIB, testFileDone, testName);

exit:
	return reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 *
 * Verify @ref omrfile.c::omrfile_stat "omrfile_stat()" correctly
 * determines if files and directories exist.
 */
TEST_F(PortFileTest2, file_test26)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test26);

	const char *localFilename = "tfileTest26.tst";
	char *knownFile = NULL;
	J9FileStat buf;
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	if (0 == omrsysinfo_get_executable_name("java.properties", &knownFile)) {
		/* NULL name */
		rc = omrfile_stat(NULL, 0, &buf);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(NULL, ..) returned %d expected %d\n", rc, -1);
		}

		/* Query a file that should not be there */
		rc = omrfile_stat(localFilename, 0, &buf);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(%s, ..) returned %d expected %d\n", localFilename, rc, -1);
		}

		/* Still should not be there */
		rc = omrfile_stat(localFilename, 0, &buf);
		if (rc >= 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(%s, ..) returned %d expected %d\n", localFilename, rc, -1);
		}

		/* A directory we know is there, how about the one we are in? */
		rc = omrfile_stat(".", 0, &buf);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(%s, ..) returned %d expected %d\n", ".", rc, 0);
		} else if (buf.isDir != 1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(%s, ..) set buf.isDir = %d expected 1\n", ".", buf.isDir);
		}

		/* Query a file that should be there  */
		rc = omrfile_stat(knownFile, 0, &buf);
		if (rc != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(%s, ..) returned %d expected %d\n", ".", rc, 0);
		} else if (buf.isFile != 1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(%s, ..) set buf.isFile = %d expected 1\n", ".", buf.isFile);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrsysinfo_get_executable_name(\"java.properties\", &knownFile) failed\n");
	}

	/* Don't delete executable name (knownFile); system-owned. */
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify chmod on files
 *
 * Verify @ref omrfile.c::omrfile_chmod "omrfile_chmod()" correctly
 * sets file permissions.
 */
TEST_F(PortFileTest2, file_test27)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test27);
	const char *localFilename = "omrfile_test27.tst";
	intptr_t fd1, fd2;
	int32_t expectedMode;
#if defined(OMR_OS_WINDOWS)
	const int32_t allWrite = 0222;
	const int32_t allRead = 0444;
#endif /* defined(OMR_OS_WINDOWS) */
	const int32_t ownerWritable = 0200;
	const int32_t ownerReadable = 0400;
	int32_t m;
	int32_t rc;
#define nModes 22
	/* do not attempt to set sticky bit (01000) on files on AIX: will fail if file is on a locally-mounted filesystem
	 * On OSX, when chmod() returns EPERM, it is called again with bits S_ISGID and S_ISUID ignored and returns success.
	 */
	int32_t testModes[nModes] = {0, 1, 2, 4, 6, 7, 010, 020, 040, 070, 0100, 0200, 0400, 0700, 0666, 0777, 0755, 0644, 02644, 04755, 06600, 04777};

	reportTestEntry(OMRPORTLIB, testName);

	omrfile_chmod(localFilename, 0777);
	omrfile_unlink(localFilename);
	fd1 = FILE_OPEN_FUNCTION(OMRPORTLIB, localFilename, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fd1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
	} else {
		(void)FILE_CLOSE_FUNCTION(OMRPORTLIB, fd1);
		for (m = 0; m < nModes; ++m) {
#if defined(AIXPPC) || defined(J9ZOS390)
			if (0 != (testModes[m] & J9S_ISGID)) {
				continue; /* sgid not support on some versions of AIX */
			}
#elif defined(OSX)
			if (0 != (testModes[m] & (J9S_ISUID | J9S_ISGID))) {
				continue; /* sgid/suid sometimes ignored on OSX */
			}
#endif /* defined(AIXPPC) || defined(J9ZOS390) */
#if defined(OMR_OS_WINDOWS)
			if (allWrite == (testModes[m] & allWrite)) {
				expectedMode = allRead + allWrite;
			} else {
				expectedMode = allRead;
			}
#else
			expectedMode = testModes[m];
#endif /* defined(OMR_OS_WINDOWS) */
			rc = omrfile_chmod(localFilename, testModes[m]);
			if (expectedMode != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_chmod() returned %d expected %d\n", rc, expectedMode);
				break;
			}
			fd2 = FILE_OPEN_FUNCTION(OMRPORTLIB, localFilename, EsOpenWrite, rc);
			if (0 == (rc & ownerWritable)) {
				if (-1 != fd2) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "opened read-only file for writing fd=%d expected -1\n", rc);
					break;
				}
			} else {
				if (-1 == fd2) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "opened owner-writable file for writing failed");
					break;
				}
			}
			FILE_CLOSE_FUNCTION(OMRPORTLIB, fd2);
			fd2 = FILE_OPEN_FUNCTION(OMRPORTLIB, localFilename, EsOpenRead, rc);
			if (0 == (rc & ownerReadable)) {
				if (-1 != fd2) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "opened write-only file for reading fd=%d expected -1\n", rc);
					break;
				}
			} else {
				if (-1 == fd2) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "opened owner-readable file for reading failed");
					break;
				}
			}
			FILE_CLOSE_FUNCTION(OMRPORTLIB, fd2);
		}
	}
	omrfile_chmod(localFilename, 0777);
	omrfile_unlink(localFilename);
	rc = omrfile_chmod(localFilename, 0755);
	if (-1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_chmod() on non-existent file returned %d expected %d\n", rc, -1);
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify chmod on directories
 *
 * Verify @ref omrfile.c::omrfile_chmod "omrfile_chmod()" correctly
 * sets file permissions.
 */
TEST_F(PortFileTest2, file_test28)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test28);
	const char *localDirectoryName = "omrfile_test28_dir";
	int32_t expectedMode;
#if defined(OMR_OS_WINDOWS)
	const int32_t allWrite = 0222;
	const int32_t allRead = 0444;
#endif /* defined(OMR_OS_WINDOWS) */
	int32_t m;
	int32_t rc;
#define nModes 22
	int32_t testModes[nModes] = {0, 1, 2, 4, 6, 7, 01010, 02020, 04040, 070, 0100, 0200, 0400, 0700, 0666, 0777, 0755, 0644, 01644, 02755, 03600, 07777};

	reportTestEntry(OMRPORTLIB, testName);

	omrfile_chmod(localDirectoryName, 0777);
	omrfile_unlinkdir(localDirectoryName);
	rc = omrfile_mkdir(localDirectoryName);
	if (-1 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed for %s: could not create\n", FILE_OPEN_FUNCTION_NAME, localDirectoryName);
	} else {
		for (m = 0; m < nModes; ++m) {
#if defined(AIXPPC) || defined(J9ZOS390)
			if (0 != (testModes[m] & J9S_ISGID)) {
				continue; /* sgid not support on some versions of AIX */
			}
#elif defined(OSX)
			if (0 != (testModes[m] & (J9S_ISUID | J9S_ISGID))) {
				continue; /* sgid/suid sometimes ignored on OSX */
			}
#endif /* defined(AIXPPC) || defined(J9ZOS390) */
#if defined(OMR_OS_WINDOWS)
			if (allWrite == (testModes[m] & allWrite)) {
				expectedMode = allRead + allWrite;
			} else {
				expectedMode = allRead;
			}
#else
			expectedMode = testModes[m];
#endif /* defined(OMR_OS_WINDOWS) */
			rc = omrfile_chmod(localDirectoryName, testModes[m]);
			if (expectedMode != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_chmod() returned %d expected %d\n", rc, expectedMode);
				break;
			}
		}
	}
	omrfile_chmod(localDirectoryName, 0777);
	omrfile_unlinkdir(localDirectoryName);

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify chown on directories
 *
 * Verify @ref omrfile.c::omrfile_chown "omrfile_chown()" correctly
 * sets file group.  Also test omrsysinfo_get_egid(), omrsysinfo_get_euid()
 */
TEST_F(PortFileTest2, file_test29)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test29);
	const char *testFile = "omrfile_test29_file";
#if !defined(OMR_OS_WINDOWS)
	uintptr_t gid, uid;
	intptr_t rc;
#endif /* !defined(OMR_OS_WINDOWS) */

	reportTestEntry(OMRPORTLIB, testName);
#if !defined(OMR_OS_WINDOWS)
	omrfile_unlink(testFile);
	(void)omrfile_create_file(OMRPORTLIB, testFile, 0, testName);
	gid = omrsysinfo_get_egid();
	rc = omrfile_chown(testFile, OMRPORT_FILE_IGNORE_ID, gid);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_chown() change group returned %d expected %d\n", rc, 0);
	}
	uid = omrsysinfo_get_euid();
	rc = omrfile_chown(testFile, uid, OMRPORT_FILE_IGNORE_ID);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_chown()set uid returned %d expected %d\n", rc, 0);
	}
	rc = omrfile_chown(testFile, 0, OMRPORT_FILE_IGNORE_ID);
	if (0 == rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_chown() to root succeeded, should have failed\n");
	}
	omrfile_unlink(testFile);
#endif /* !defined(OMR_OS_WINDOWS) */

	reportTestExit(OMRPORTLIB, testName);
}



/**
 *
 * Verify @ref omrfile.c::omrfile_open returns OMRPORT_ERROR_FILE_ISDIR if we are trying to create a directory.
 */
TEST_F(PortFileTest2, file_test30)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test30);
	const char *validFilename = "tfileTest27.30";

	const char *illegalFileName = "../";
	intptr_t validFileFd, existingFileFd, invalidFileFd;
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	/* 1. First we want to trigger an error so that we know omrerror_last_error() has been set
	 * 	Do this by trying to create the same file twice
	 * 2. Then try to create a directory {../)
	 *  Make sure omrerror_last_error_number() returns OMRPORT_ERROR_FILE_ISDIR
	 */

	/* create a file that doesn't exist, this should succeed */
	validFileFd = FILE_OPEN_FUNCTION(OMRPORTLIB, validFilename, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate, 0666);
	if (-1 == validFileFd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to create file %s: error_last_error_number %i\n", FILE_OPEN_FUNCTION_NAME, validFilename, omrerror_last_error_number());
		goto exit;
	}

	/* try to create the file again, this should fail. The only reason we do this is to have omrfile set the last error code */
	existingFileFd = FILE_OPEN_FUNCTION(OMRPORTLIB, validFilename, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate, 0666);
	if (-1 != existingFileFd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s should have failed to create existing file: %s\n", FILE_OPEN_FUNCTION_NAME, validFilename);
		goto exit;
	}

	/* try opening a "../" file, expect failure (write open) */
	invalidFileFd = FILE_OPEN_FUNCTION(OMRPORTLIB, illegalFileName, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate, 0666);
	if (-1 == invalidFileFd) {
		int32_t lastErrorNumber = omrerror_last_error_number();

		if (lastErrorNumber == OMRPORT_ERROR_FILE_EXIST) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_number(\"%s\") returned %i (OMRPORT_ERROR_FILE_EXIST), anything but would be fine %i\n", illegalFileName, lastErrorNumber);
		}

	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s(\"%s\") did not return failure.\n", FILE_OPEN_FUNCTION_NAME, illegalFileName);
	}

	/* try opening a "../" file, expect failure (read open) */
	invalidFileFd = FILE_OPEN_FUNCTION(OMRPORTLIB, illegalFileName, EsOpenCreateNew | EsOpenRead | EsOpenTruncate, 0666);
	if (-1 == invalidFileFd) {
		int32_t lastErrorNumber = omrerror_last_error_number();

		if (lastErrorNumber == OMRPORT_ERROR_FILE_EXIST) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_number(\"%s\") returned %i (OMRPORT_ERROR_FILE_EXIST), anything but would be fine %i\n", illegalFileName, lastErrorNumber);
		}

	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s(\"%s\") did not return failure.\n", FILE_OPEN_FUNCTION_NAME, illegalFileName);
	}

exit:

	/* Close the file references, should succeed */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, validFileFd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}

	/* Delete this file, should succeed */
	rc = omrfile_unlink(validFilename);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() returned %d expected %d\n", rc, -1);
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Test file owner and group attributes
 */
TEST_F(PortFileTest2, file_test31)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test31);
	const char *filename = "tfileTest31";
	intptr_t fileFd;
	uintptr_t myUid, myGid, fileGid;
	J9FileStat stat;
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	omrfile_unlink(filename); /* ensure there is no stale file */
	fileFd = FILE_OPEN_FUNCTION(OMRPORTLIB, filename, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate, 0666);
	if (-1 == fileFd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to create file %s: error_last_error_number %i\n", FILE_OPEN_FUNCTION_NAME,
						   filename, omrerror_last_error_number());
		goto exit;
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
exit:

	/* Close the file references, should succeed */
	rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fileFd);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
	}

	/* Delete this file, should succeed */
	rc = omrfile_unlink(filename);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() returned %d expected %d\n", rc, -1);
	}

	reportTestExit(OMRPORTLIB, testName);
}

TEST_F(PortFileTest2, file_test32)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test32_test_file_permission_bits_using_omrfile_stat());
	const char *localFilename = "tfileTest32";
	intptr_t fd;
	J9FileStat stat;

	reportTestEntry(OMRPORTLIB, testName);
	portTestEnv->changeIndent(1);

	/*
	 * Test a read-writeable file
	 */
	portTestEnv->log("omrfile_stat() Test a read-writeable file\n");
	omrfile_unlink(localFilename);
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, localFilename, EsOpenCreate | EsOpenWrite, 0666);
	if (fd == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() file open failed\n", FILE_OPEN_FUNCTION_NAME);
		goto done;
	}
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
#if defined(OMR_OS_WINDOWS)
	/*The mode param is ignored by omrfile_open, so omrfile_chmod() is called*/
	omrfile_chmod(localFilename, 0666);
#endif /* defined(OMR_OS_WINDOWS) */
	omrfile_stat(localFilename, 0, &stat);

	if (stat.perm.isUserWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isUserWriteable != 1'\n");
	}
	if (stat.perm.isUserReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isUserReadable != 1'\n");
	}
#if defined(OMR_OS_WINDOWS)
	if (stat.perm.isGroupWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isGroupWriteable != 1'\n");
	}
	if (stat.perm.isGroupReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isGroupReadable != 1'\n");
	}
	if (stat.perm.isOtherWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isOtherWriteable != 1'\n");
	}
	if (stat.perm.isOtherReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isOtherReadable != 1'\n");
	}
#else
	/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
#endif /* defined(OMR_OS_WINDOWS) */

	omrfile_unlink(localFilename);

	/*
	 * Test a readonly file
	 */
	portTestEnv->log("omrfile_stat() Test a readonly file\n");
	omrfile_unlink(localFilename);
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, localFilename, EsOpenCreate | EsOpenRead, 0444);

	if (fd == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() file open failed\n", FILE_OPEN_FUNCTION_NAME);
		goto done;
	}
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
#if defined(OMR_OS_WINDOWS)
	/*The mode param is ignored by omrfile_open, so omrfile_chmod() is called*/
	omrfile_chmod(localFilename, 0444);
#endif /* defined(OMR_OS_WINDOWS) */
	omrfile_stat(localFilename, 0, &stat);

	if (stat.perm.isUserWriteable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isUserWriteable != 0'\n");
	}
	if (stat.perm.isUserReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isUserReadable != 1'\n");
	}
#if defined(OMR_OS_WINDOWS)
	if (stat.perm.isGroupWriteable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isGroupWriteable != 0'\n");
	}
	if (stat.perm.isGroupReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isGroupReadable != 1'\n");
	}
	if (stat.perm.isOtherWriteable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isOtherWriteable != 0'\n");
	}
	if (stat.perm.isOtherReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isOtherReadable != 1'\n");
	}
#else
	/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
#endif /* defined(OMR_OS_WINDOWS) */

	omrfile_unlink(localFilename);

	/*
	 * Test a writeable file
	 */
	portTestEnv->log("omrfile_stat() Test a writeable file\n");
	omrfile_unlink(localFilename);
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, localFilename, EsOpenCreate | EsOpenWrite, 0222);
	if (fd == -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() file open failed\n", FILE_OPEN_FUNCTION_NAME);
		goto done;
	}
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
#if defined(OMR_OS_WINDOWS)
	/*The mode param is ignored by omrfile_open, so omrfile_chmod() is called*/
	omrfile_chmod(localFilename, 0222);
#endif /* defined(OMR_OS_WINDOWS) */
	omrfile_stat(localFilename, 0, &stat);

	if (stat.perm.isUserWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isUserWriteable != 1'\n");
	}
#if defined(OMR_OS_WINDOWS)
	/*On windows if a file is write-able, then it is also read-able*/
	if (stat.perm.isUserReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isUserReadable != 1'\n");
	}
#else
	if (stat.perm.isUserReadable != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isUserReadable != 0'\n");
	}
#endif /* defined(OMR_OS_WINDOWS) */

#if defined(OMR_OS_WINDOWS)
	if (stat.perm.isGroupWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isGroupWriteable != 1'\n");
	}
	/*On windows if a file is write-able, then it is also read-able*/
	if (stat.perm.isGroupReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isGroupReadable != 1'\n");
	}
	if (stat.perm.isOtherWriteable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isOtherWriteable != 1'\n");
	}
	/*On windows if a file is write-able, then it is also read-able*/
	if (stat.perm.isOtherReadable != 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat() error 'stat.isOtherReadable != 1'\n");
	}
#else
	/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
#endif /* defined(OMR_OS_WINDOWS) */


done:
	omrfile_unlink(localFilename);
	portTestEnv->changeIndent(-1);
	reportTestExit(OMRPORTLIB, testName);
}


TEST_F(PortFileTest2, file_test33)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

const char *testName = APPEND_ASYNC(omrfile_test33: test UTF - 8 encoding);

	const unsigned char single_byte_utf8[] = {0x24, '\n'}; /* U+0024 $ */
	const unsigned char two_byte_utf8[] = {0xC2, 0xA2, '\n'}; /* U+00A2 ???*/
	const unsigned char three_byte_utf8[] = {0xE2, 0x82, 0xAC, '\n'}; /* U+20AC ???*/
	const unsigned char four_byte_utf8[] = {0xF0, 0xA4, 0xAD, 0xA2, '\n'}; /* U+024B62 beyond BMP */
	const unsigned char invalid_byte_utf8[] = {0xFE, 0xFF, '\n'}; /* Invalid */

	size_t i = 0;
	unsigned char many_byte_utf8[512];

	for (i = 0; i < 508; i++) {
		many_byte_utf8[i] = '*';
	}
	many_byte_utf8[i++] = 0xE2;
	many_byte_utf8[i++] = 0x82;
	many_byte_utf8[i++] = 0xAC;
	many_byte_utf8[i] = '\n'; /* force outbuf to be too small for converted unicode format */

	reportTestEntry(OMRPORTLIB, testName);

	omrfile_write_text(1, (const char *)single_byte_utf8, sizeof(char) * 2);
	omrfile_write_text(1, (const char *)two_byte_utf8, sizeof(char) * 3);
	omrfile_write_text(1, (const char *)three_byte_utf8, sizeof(char) * 4);		/* Unicode string representation should be printed */
	omrfile_write_text(1, (const char *)four_byte_utf8, sizeof(char) * 5);		/* '????' should be printed */
	omrfile_write_text(1, (const char *)invalid_byte_utf8, sizeof(char) * 3);	/* '??' should be printed */
	omrfile_write_text(1, (const char *)many_byte_utf8, sizeof(char) * 512);	/* Unicode string representation should be printed */

	reportTestExit(OMRPORTLIB, testName);
}

#if defined(OMR_OS_WINDOWS)
/**
 * Test a very long file name
 *
 * For both relative and absolute paths, build up a directory hierarchy that is greater than the system defined MAX_PATH
 *
 * Note that this test is only run on windows, as it is the only OS (AFAIK)
 * that supports file names longer than 256 chars.
 */
TEST_F(PortFileTest2, file_test_long_file_name)
{
	/* first we need a file to copy use with vprintf */
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test_long_file_name);
	int32_t rc, mkdirRc, unlinkRc;
	char expectedResult[] = "Try a number - 1Try a string - abcTry a mixture - 1 abc 2";
	char readBuf[255];
	char *readBufPtr;
	char *longDirName = "\\abcdefghijklmnopqrstuvwx";
#define MIN_LENGTH (MAX_PATH + 1)
#define FILENAME_LENGTH MIN_LENGTH*2 /* double the size to accomodate a file size that is just under twice MIN_LENGTH */
	char filePathName[FILENAME_LENGTH];
	intptr_t fd;
	char cwd[FILENAME_LENGTH];
	char *basePaths[2];
	int i;

	reportTestEntry(OMRPORTLIB, testName);

	/* get the current drive designator for the absolute path*/
	_getcwd(cwd, FILENAME_LENGTH);
	cwd[2] = '\0'; /* now we have something like "c:" */

	basePaths[0] = "."; /* to test a relative path */
	basePaths[1] = cwd; /* to test an absolute path */

	for (i = 0; i < 2; i ++) {
		omrstr_printf(filePathName, FILENAME_LENGTH, "%s", basePaths[i]);

		/* build up a file name that is longer than 256 characters,
		 * comprised of directories, each of which are less than 256 characters in length*/
		while (strlen(filePathName) < MIN_LENGTH) {

			omrstr_printf(filePathName + strlen(filePathName), FILENAME_LENGTH - strlen(filePathName), "%s", longDirName);

			mkdirRc = omrfile_mkdir(filePathName);

			if ((mkdirRc != 0)) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_mkdir() failed for dir with length %i: %s\n", strlen(filePathName), filePathName);
				goto exit;
			}
		}

		/* now append filePathName with the actual filename */
		omrstr_printf(filePathName + strlen(filePathName), FILENAME_LENGTH - strlen(filePathName), "\\%s", testName);

		portTestEnv->log("\ttesting filename: %s\n", filePathName);

		/* can we open and write to the file? */
		fd = FILE_OPEN_FUNCTION(OMRPORTLIB, filePathName, EsOpenCreate | EsOpenWrite, 0666);
		if (-1 == fd) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
			goto exit;
		}

		fileWrite(OMRPORTLIB, fd, "Try a number - %d", 1);
		fileWrite(OMRPORTLIB, fd, "Try a string - %s", "abc");
		fileWrite(OMRPORTLIB, fd, "Try a mixture - %d %s %d", 1, "abc", 2);

		/* having hopefully shoved this stuff onto file we need to read it back and check its as expected! */
		rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
			goto exit;
		}

		fd = FILE_OPEN_FUNCTION(OMRPORTLIB, filePathName, EsOpenRead, 0444);
		if (-1 == fd) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
			goto exit;
		}

		readBufPtr = omrfile_read_text(fd, readBuf, sizeof(readBuf));
		if (NULL == readBufPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read_text() returned NULL\n");
			goto exit;
		}

		if (strlen(readBufPtr) != strlen(expectedResult)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read_text() returned %d expected %d\n", strlen(readBufPtr), strlen(expectedResult));
			goto exit;
		}

		/* omrfile_read_text(): translation from IBM1047 to ASCII is done on zOS */
		if (strcmp(readBufPtr, expectedResult)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_read_text() returned \"%s\" expected \"%s\"\n", readBufPtr , expectedResult);
			goto exit;
		}

exit:
		FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);

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
 * Test omrfile_stat_filesystem ability to measure free disk space
 *
 * This test check the amount of free disk space, create a file and check the amount of free disk space again.
 * It is necessary for the amount of free disk space to changes after the file is created.
 * It is necessary for the total amount of disk space to be constant after the file is created.
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
#if !defined(OMR_OS_WINDOWS)
int
omrfile_test34(struct OMRPortLibrary *portLibrary)
{
#define J9FILETEST_FILESIZE 1000 * 512
	J9FileStatFilesystem buf_before, buf_after;
	J9FileStat buf;
	intptr_t rc, fd;
	uintptr_t freeSpaceDelta;
	char *stringToWrite = NULL;
	int returnCode = TEST_PASS;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const char *testName = APPEND_ASYNC(omrfile_test34);
	const char *fileName = "omrfile_test34.tst";

	reportTestEntry(OMRPORTLIB, testName);
	portTestEnv->changeIndent(1);

	/* delete fileName to have a clean test, ignore the result */
	(void)omrfile_unlink(fileName);

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenCreate | EsOpenRead | EsOpenWrite, 0666);
	if (-1 == fd) {
		portTestEnv->log(LEVEL_ERROR, "%s() failed\n", FILE_OPEN_FUNCTION_NAME);
		returnCode = TEST_FAIL;
		goto exit;
	}

#if defined(LINUX) || defined(OSX)
	sync();
#endif /* defined(LINUX) || defined(OSX) */
	rc = omrfile_stat_filesystem(fileName, 0, &buf_before);
	if (0 != rc) {
		portTestEnv->log(LEVEL_ERROR, "first omrfile_stat_filesystem() returned %d, expected %d\n", rc, 0);
		returnCode = TEST_FAIL;
		goto exit;
	}
	rc = omrfile_stat(fileName, 0, &buf);
	if (0 != rc) {
		portTestEnv->log(LEVEL_ERROR, "first omrfile_stat() returned %d, expected %d\n", rc, 0);
		returnCode = TEST_FAIL;
		goto exit;
	}

	if (buf.isRemote) {
		portTestEnv->log(LEVEL_WARN, "WARNING: Test omrfile_test34 cannot be run on a remote file system.\nSkipping test.\n");
		returnCode = TEST_PASS;
		goto exit;
	}

	stringToWrite = (char *)omrmem_allocate_memory(J9FILETEST_FILESIZE, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == stringToWrite) {
		portTestEnv->log(LEVEL_ERROR, "Failed to allocate %zu bytes\n", J9FILETEST_FILESIZE);
		returnCode = TEST_FAIL;
		goto exit;
	}
	memset(stringToWrite, 'a', J9FILETEST_FILESIZE);
	stringToWrite[J9FILETEST_FILESIZE - 1] = '\0';

	omrfile_printf(fd, "%s", stringToWrite);
	omrfile_sync(fd); /* flush data written to disk */

#if defined(LINUX) || defined(OSX)
	sync();
#endif /* defined(LINUX) || defined(OSX) */
	rc = omrfile_stat_filesystem(fileName, 0, &buf_after);
	if (0 != rc) {
		portTestEnv->log(LEVEL_ERROR, "second omrfile_stat_filesystem() returned %d, expected %d\n", rc, 0);
		returnCode = TEST_FAIL;
		goto exit;
	}

	freeSpaceDelta = (uintptr_t)(buf_before.freeSizeBytes - buf_after.freeSizeBytes);

	/* Checks if the free disk space changed after the file has been created. */
	if (0 == freeSpaceDelta) {
		portTestEnv->log("The amount of free disk space does not change after the creation of a file.\n\
			It can be due to a file of exactly %d bytes being deleted during this test, or omrfile_stat_filesystem failing.\n\
			The test does not work on a remote file system. Ignore failures on remote file systems.\n", J9FILETEST_FILESIZE);
		returnCode = TEST_FAIL;
		goto exit;
	}

	if (buf_before.totalSizeBytes != buf_after.totalSizeBytes) {
		portTestEnv->log("Total disk space changed between runs, it was %llu changed to %llu.\n\
				The disk space should not change between runs.\n", buf_before.totalSizeBytes, buf_after.totalSizeBytes);
		returnCode = TEST_FAIL;
		goto exit;
	}

exit:

	/* Close the file, expect success */
	if (-1 != fd) {
		rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
		if (0 != rc) {
			portTestEnv->log(LEVEL_ERROR, "%s() returned %d expected %d\n", FILE_CLOSE_FUNCTION_NAME, rc, 0);
		}

		/* Delete the file, expect success */
		rc = omrfile_unlink(fileName);
		if (0 != rc) {
			portTestEnv->log(LEVEL_ERROR, "omrfile_unlink() returned %d expected %d\n", rc, 0);
		}
	}

	omrmem_free_memory(stringToWrite);

	portTestEnv->changeIndent(-1);
	reportTestExit(OMRPORTLIB, testName);

	return returnCode;
}

/**
 * Test omrfile_test34 nbOfTries times
 *
 * @return TEST_PASS as soon as the test passes, TEST_FAIL if the test always fails
 */
int
omrfile_test34_multiple_tries(struct OMRPortLibrary *portLibrary, int nbOfTries)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	int i, rc;
const char *testName = APPEND_ASYNC(omrfile_test34_multiple_tries: _test_omrfile_stat_filesystem_disk_space);

	reportTestEntry(OMRPORTLIB, testName);
	/* The test omrfile_test34 may fail if other processes writes to the hard drive, trying the test nbOfTries times */
	for (i = 0; i < nbOfTries; i++) {
		rc = omrfile_test34(portLibrary);
		if (TEST_PASS == rc) {
			goto exit;
		}
		omrthread_sleep(100);
	}
	outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_test34_multiple_tries failed %d time(s)\n", nbOfTries);
exit:
	return reportTestExit(OMRPORTLIB, testName);
}
#endif /* !defined(OMR_OS_WINDOWS) */

/**
 * Test omrfile_test35
 * Tests omrfile_flength()
 */
TEST_F(PortFileTest2, file_test35)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	const char *testName = APPEND_ASYNC(omrfile_test35);
	const char *fileName = "omrfile_test35.txt";
	intptr_t fd;
	I_64 fileLength = 1024, seekOffset = 10, offset, newLength, newfLength;
	intptr_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenCreate | EsOpenWrite, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned -1 expected valid file handle", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	rc = FILE_SET_LENGTH_FUNCTION(OMRPORTLIB, fd, fileLength);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %d\n", FILE_SET_LENGTH_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	newLength = omrfile_length(fileName);
	if (newLength != fileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_length() returned %lld expected %lld\n", newLength, fileLength);
		goto exit;
	}

	newfLength = FILE_FLENGTH_FUNCTION(OMRPORTLIB, fd);
	if (newfLength != fileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %lld\n", FILE_FLENGTH_FUNCTION_NAME, newfLength, fileLength);
		goto exit;
	}
	if (newfLength != newLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %lld\n", FILE_FLENGTH_FUNCTION_NAME, newfLength, newLength);
		goto exit;
	}

	/* Testing that omrfile_flength doesn't change the file_seek offset */
	offset = omrfile_seek(fd, seekOffset, EsSeekSet);
	if (seekOffset != offset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_seek() returned %lld expected %lld\n", offset, seekOffset);
		goto exit;
	}

	newfLength = FILE_FLENGTH_FUNCTION(OMRPORTLIB, fd);
	if (newfLength != fileLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %lld\n", FILE_FLENGTH_FUNCTION_NAME, newfLength, fileLength);
		goto exit;
	}

	offset = omrfile_seek(fd, 0, EsSeekCur);
	if (seekOffset != offset) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "file_seek() returned %lld expected %lld\n", offset, seekOffset);
		goto exit;
	}
exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	omrfile_unlink(fileName);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Test omrfile_test36
 *
 * Simple test of omrfile_convert_native_fd_to_omrfile_fd (it's a simple function!)
 */
TEST_F(PortFileTest2, file_test36)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test36);

	intptr_t nativeFD = 999; /* pick an arbitrary number */
	intptr_t omrfileFD = -1;

	reportTestEntry(OMRPORTLIB, testName);

	omrfileFD = omrfile_convert_native_fd_to_omrfile_fd(nativeFD);

#if !defined(J9ZOS390)
	if (omrfileFD != nativeFD) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfileFD != nativeFD, they should be the same. omrfileFD: %x, nativeFD: %x\n", omrfileFD, nativeFD);
	}
#else /* defined(J9ZOS390) */
	if (omrfileFD == nativeFD) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfileFD == nativeFD, they should be different. omrfileFD: %x, nativeFD: %x\n", omrfileFD, nativeFD);
	}

#endif /* defined(J9ZOS390) */

	/* go back and forth with a standard stream.  */
	nativeFD = omrfile_convert_omrfile_fd_to_native_fd(OMRPORT_TTY_OUT);
	if (nativeFD < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "nativeFD < 0 , should be non-negative. nativeFD: %x\n", nativeFD);
	}

	omrfileFD = omrfile_convert_native_fd_to_omrfile_fd(nativeFD);
	if (omrfileFD == OMRPORT_INVALID_FD) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9fileFD == J9PORT_INVALID_FD, they should be the same. j9fileFD: %x\n", omrfileFD);
	}
	if (omrfileFD != OMRPORT_TTY_OUT) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9fileFD != J9PORT_TTY_OUT, they should be the same. j9fileFD: %x\n", omrfileFD);
	}

	reportTestExit(OMRPORTLIB, testName);
}

#if !defined(OMR_OS_WINDOWS)
/**
 * Verify port file system.
 *
 * Verify @ref omrfile.c::omrfile_fstat "omrfile_fstat()" correctly
 * determines if files and directories exist.
 * Similar to omrfile_test26 for omrfile_stat().
 */
TEST_F(PortFileTest2, file_test37)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test37);
	char *knownFile = NULL;
	J9FileStat buf;
	int32_t rc = -1;

	reportTestEntry(OMRPORTLIB, testName);

	memset(&buf, 0, sizeof(J9FileStat));

	/* Invalid fd */
	rc = omrfile_fstat(-1, &buf);
	if (rc >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Negative test failure: omrfile_fstat(-1, ..) on invalid file descriptor is expected to fail but it returned success with rc=%d (ignore the following LastErrorNumber and LastErrorMessage)\n", rc);
	}

	if (0 == omrsysinfo_get_executable_name(NULL, &knownFile)) {
		/* Query a file that should be there  */
		intptr_t fd = FILE_OPEN_FUNCTION(OMRPORTLIB, knownFile, EsOpenRead, 0666);
		if (-1 != fd) {
			rc = omrfile_fstat(fd, &buf);
			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat(...) failed for fd=%d(filename=%s)\n", fd, knownFile);
			} else if (1 != buf.isFile) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat(...) for %s returned buf.isFile = %d, expected 1\n", knownFile, buf.isFile);
			}
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to open file %s\n", FILE_OPEN_FUNCTION_NAME, knownFile);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrsysinfo_get_executable_name(NULL, &knownFile) failed\n");
	}

	/* Don't delete executable name (knownFile); system-owned. */
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Test file owner and group attributes using omrfile_fstat()
 * Similar to omrfile_test31 for omrfile_stat().
 */
TEST_F(PortFileTest2, file_test38)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test38);
	const char *filename = "tfileTest38.tst";
	intptr_t fd = -1;
	uintptr_t myUid = 0;
	uintptr_t myGid = 0;
	uintptr_t fileGid = 0;
	J9FileStat stat;
	int32_t rc = -1;

	reportTestEntry(OMRPORTLIB, testName);

	memset(&stat, 0, sizeof(J9FileStat));

	omrfile_unlink(filename); /* ensure there is no stale file */
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, filename, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate, 0666);
	if (-1 != fd) {
		rc = omrfile_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			myUid = omrsysinfo_get_euid();
			myGid = omrsysinfo_get_egid();
			fileGid = stat.ownerGid;

			if (myUid != stat.ownerUid) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat(...) for %s returned ownerUid as %d, expected %d\n", filename, stat.ownerUid, myUid);
			}

			if (myGid != fileGid) {
				portTestEnv->log("Unexpected group ID. Possibly setgid bit set. Comparing to group of the current directory\n");
				rc = omrfile_stat(".", 0, &stat);
				if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_stat(...) failed for current directory with rc=%d\n", rc);
				} else {
					if (fileGid != stat.ownerGid) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat(...) for current directory returned ownerGid as %d, expected %d\n", fileGid, stat.ownerGid);
					}
				}
			}
		}

		/* Close the file references, should succeed */
		rc = FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to close the file\n", FILE_CLOSE_FUNCTION_NAME);
		}

		/* Delete this file, should succeed */
		rc = omrfile_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() failed to delete file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s failed to create file %s\n", FILE_OPEN_FUNCTION_NAME, filename);
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Test file file permission bits using omrfile_fstat()
 * Similar to omrfile_test32 for omrfile_stat().
 */
TEST_F(PortFileTest2, file_test39)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
const char *testName = APPEND_ASYNC(omrfile_test39: test file permission bits using omrfile_fstat());
	const char *filename = "tfileTest39.tst";
	intptr_t fd = -1;
	J9FileStat stat;
	int32_t mode = 0;
	int32_t rc = -1;

	reportTestEntry(OMRPORTLIB, testName);
	portTestEnv->changeIndent(1);

	memset(&stat, 0, sizeof(J9FileStat));

	/*
	 * Test a read-writeable file
	 */
	portTestEnv->log("omrfile_fstat() Test a read-writeable file\n");
	omrfile_unlink(filename);

	mode = 0666;
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, filename, EsOpenCreate | EsOpenWrite, mode);
	if (-1 != fd) {
		rc = omrfile_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserWriteable != 1'\n");
			}
			if (stat.perm.isUserReadable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserReadable != 1'\n");
			}
			/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
		}

		FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
		rc = omrfile_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() failed to delete the file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}

	/*
	 * Test a read-only file
	 */
	portTestEnv->log("omrfile_fstat() Test a read-only file\n");

	mode = 0444;
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, filename, EsOpenCreate | EsOpenRead, mode);
	if (-1 != fd) {
		rc = omrfile_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserWriteable != 0'\n");
			}
			if (stat.perm.isUserReadable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserReadable != 1'\n");
			}
			/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
		}

		FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
		rc = omrfile_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() failed to delete the file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}

	/*
	 * Test a write-only file
	 */
	portTestEnv->log("omrfile_fstat() Test a write-only file\n");

	mode = 0222;
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, filename, EsOpenCreate | EsOpenWrite, mode);
	if (-1 != fd) {
		rc = omrfile_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserWriteable != 1'\n");
			}
			if (stat.perm.isUserReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserReadable != 0'\n");
			}
			/*umask vary so we can't test 'isGroupWriteable', 'isGroupReadable', 'isOtherWriteable', or 'isOtherReadable'*/
		}

		FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
		rc = omrfile_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() failed to delete the file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}

	/*
	 * Test a user only read-writeable file
	 */
	portTestEnv->log("omrfile_fstat() Test a user only read-writeable file\n");
	omrfile_unlink(filename);

	mode = 0600;
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, filename, EsOpenCreate | EsOpenWrite, mode);
	if (-1 != fd) {
		rc = omrfile_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserWriteable != 1'\n");
			}
			if (stat.perm.isUserReadable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserReadable != 1'\n");
			}
			if (stat.perm.isGroupWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isGroupWriteable != 0'\n");
			}
			if (stat.perm.isGroupReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isGroupReadable != 0'\n");
			}
			if (stat.perm.isOtherWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isOtherWriteable != 0'\n");
			}
			if (stat.perm.isOtherReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isOtherReadable != 0'\n");
			}
		}

		FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
		rc = omrfile_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() failed to delete the file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "5s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}

	/*
	 * Test a user only read-only file
	 */
	portTestEnv->log("omrfile_fstat() Test a user only read-only file\n");

	mode = 0400;
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, filename, EsOpenCreate | EsOpenRead, mode);
	if (-1 != fd) {
		rc = omrfile_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserWriteable != 0'\n");
			}
			if (stat.perm.isUserReadable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserReadable != 1'\n");
			}
			if (stat.perm.isGroupWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isGroupWriteable != 0'\n");
			}
			if (stat.perm.isGroupReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isGroupReadable != 0'\n");
			}
			if (stat.perm.isOtherWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isOtherWriteable != 0'\n");
			}
			if (stat.perm.isOtherReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isOtherReadable != 0'\n");
			}
		}

		FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
		rc = omrfile_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() failed to delete the file %s\n", filename);
		}
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}

	/*
	 * Test a user only write-only file
	 */
	portTestEnv->log("omrfile_fstat() Test a user only write-only file\n");

	mode = 0200;
	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, filename, EsOpenCreate | EsOpenWrite, mode);
	if (-1 != fd) {
		rc = omrfile_fstat(fd, &stat);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat(...) failed for fd=%d (filename=%s)\n", fd, filename);
		} else {
			if (stat.perm.isUserWriteable != 1) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserWriteable != 1'\n");
			}
			if (stat.perm.isUserReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isUserReadable != 0'\n");
			}
			if (stat.perm.isGroupWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isGroupWriteable != 0'\n");
			}
			if (stat.perm.isGroupReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isGroupReadable != 0'\n");
			}
			if (stat.perm.isOtherWriteable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isOtherWriteable != 0'\n");
			}
			if (stat.perm.isOtherReadable != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_fstat() error 'stat.isOtherReadable != 0'\n");
			}
		}

		FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
		rc = omrfile_unlink(filename);
		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_unlink() failed to delete the file %s\n", filename);
		}

	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "5s() failed to create file %s with mode=%d\n", FILE_OPEN_FUNCTION_NAME, filename, mode);
	}

	portTestEnv->changeIndent(-1);
	reportTestExit(OMRPORTLIB, testName);
}
#endif /* defined(OMR_OS_WINDOWS) */



/**
 * Verify port file system.
 * @ref omrfile.c::omrfile_set_length "omrfile_set_length()"
 */
TEST_F(PortFileTest2, file_test40)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(omrfile_test40);
	const char *fileName = "tfileTest40.tst";
	I_64 filePtr = 0;
	I_64 prevFilePtr = 0;
	intptr_t fd = 0;
	char inputLine1[] = "ABCDE";
	intptr_t expectedReturnValue;
	intptr_t rc = 0;


	reportTestEntry(OMRPORTLIB, testName);
	expectedReturnValue = sizeof(inputLine1) - 1; /* don't want the final 0x00 */

	fd = FILE_OPEN_FUNCTION(OMRPORTLIB, fileName, EsOpenCreate | EsOpenWrite | EsOpenAppend, 0666);
	if (-1 == fd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() failed \n", FILE_OPEN_FUNCTION_NAME);
		goto exit;
	}

	prevFilePtr = omrfile_seek(fd, 0, EsSeekCur);
	if (0 != prevFilePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after opening file. Expected = 0, Got = %lld", prevFilePtr);
	}

	/* now lets write to it */
	rc = FILE_WRITE_FUNCTION(OMRPORTLIB, fd, inputLine1, expectedReturnValue);

	if (rc != expectedReturnValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %d expected %d\n", FILE_WRITE_FUNCTION_NAME, rc, expectedReturnValue);
		goto exit;
	}

	filePtr = omrfile_seek(fd, 0, EsSeekCur);
	if (prevFilePtr + expectedReturnValue != filePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "File pointer location is wrong after writing into file. Expected = %lld, Got = %lld", prevFilePtr + expectedReturnValue,  filePtr);
	}

	prevFilePtr = filePtr;
	rc = FILE_SET_LENGTH_FUNCTION(OMRPORTLIB, fd, 100);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() returned %lld expected %d\n", FILE_SET_LENGTH_FUNCTION_NAME, rc, 0);
		goto exit;
	}

	filePtr = omrfile_seek(fd, 0, EsSeekCur);

	if (prevFilePtr != filePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "%s() Changed the location of file pointer. Expected : %lld. Got : %lld \n", FILE_SET_LENGTH_FUNCTION_NAME, prevFilePtr, filePtr);
		goto exit; /* Although we dont need this, lets keep it to make it look like other failure cases */
	}

exit:
	FILE_CLOSE_FUNCTION(OMRPORTLIB, fd);
	omrfile_unlink(fileName);
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify omrfile_lastmod() returns -1 on an invalid file.
 * @ref omrfile.c::omrfile_lastmod "omrfile_lastmod()"
 */
TEST_F(PortFileTest2, lastmod_failedToFindFile)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = APPEND_ASYNC(lastmod_failedToFindFile);

	reportTestEntry(OMRPORTLIB, testName);

	const char *fileName = "lastmod_failedToFindFile.tst";
	omrfile_unlink(fileName);

	int64_t rc = omrfile_lastmod(fileName);
	if (rc != -1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrfile_lastmod() did not return -1 on an invalid file.");
	}
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port file system.
 *
 * Exercise all API related to port library memory management found in
 * @ref omrmem.c
 *
 * @param[in] portLibrary The port library under test
 *
 * @return 0 on success, -1 on failure
 */
int32_t
omrfile_runTests(struct OMRPortLibrary *portLibrary, char *argv0, char *omrfile_child, BOOLEAN async)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	isAsync = async;

	if (TRUE == isAsync) {
		HEADING(OMRPORTLIB, "File Blockingasync Test");
	} else {
		HEADING(OMRPORTLIB, "File Test");
	}

	if (omrfile_child != NULL) {
		if (strcmp(omrfile_child, "omrfile_test20_1") == 0) {
			return omrfile_test20_child_1(OMRPORTLIB);
		} else if (strcmp(omrfile_child, "omrfile_test20_2") == 0) {
			return omrfile_test20_child_2(OMRPORTLIB);
		} else if (strcmp(omrfile_child, "omrfile_test21_1") == 0) {
			return omrfile_test21_child_1(OMRPORTLIB);
		} else if (strcmp(omrfile_child, "omrfile_test21_2") == 0) {
			return omrfile_test21_child_2(OMRPORTLIB);
		} else if (strcmp(omrfile_child, "omrfile_test22_1") == 0) {
			return omrfile_test22_child_1(OMRPORTLIB);
		} else if (strcmp(omrfile_child, "omrfile_test22_2") == 0) {
			return omrfile_test22_child_2(OMRPORTLIB);
		} else if (strcmp(omrfile_child, "omrfile_test23_1") == 0) {
			return omrfile_test23_child_1(OMRPORTLIB);
		} else if (strcmp(omrfile_child, "omrfile_test23_2") == 0) {
			return omrfile_test23_child_2(OMRPORTLIB);
		} else if (strcmp(omrfile_child, "omrfile_test24_1") == 0) {
			return omrfile_test24_child_1(OMRPORTLIB);
		} else if (strcmp(omrfile_child, "omrfile_test24_2") == 0) {
			return omrfile_test24_child_2(OMRPORTLIB);
		} else if (strcmp(omrfile_child, "omrfile_test25_1") == 0) {
			return omrfile_test25_child_1(OMRPORTLIB);
		} else if (strcmp(omrfile_child, "omrfile_test25_2") == 0) {
			return omrfile_test25_child_2(OMRPORTLIB);
		}
	}
	return TEST_PASS;
}
