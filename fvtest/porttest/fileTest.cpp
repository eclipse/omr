/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
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

#include "omrcfg.h"

#include <limits.h>
#if defined(WIN32)
/* Undefine the winsockapi because winsock2 defines it.  Removes warnings. */
#if defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#undef _WINSOCKAPI_
#endif
#include <winsock2.h>
#endif /* defined(WIN32) */
#include "omrTest.h"
#include "omrport.h"
#include "portTestHelpers.hpp"

class PortFileTest: public ::testing::Test
{
protected:
	virtual void
	SetUp()
	{
		::testing::Test::SetUp();
#if defined(WIN32)
		/* initialize sockets so that we can use gethostname() */
		WORD wsaVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;
		int wsaErr = WSAStartup(wsaVersionRequested, &wsaData);
		ASSERT_EQ(0, wsaErr);
#endif /* defined(WIN32) */

		/* generate machine specific file name to prevent conflict between multiple tests running on shared drive */
		OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
		char hostname[512];
		gethostname(hostname, 512);
		const uintptr_t fileNameLen = 1024;
		fileName = new char[fileNameLen];
		omrstr_printf(fileName, fileNameLen, "TestFileFor%s%s", ::testing::UnitTest::GetInstance()->current_test_info()->name(), hostname);
	}

	virtual void
	TearDown()
	{
		OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
		omrfile_close(fileDescriptor);
		omrfile_unlink(fileName);
		delete[] fileName;

#if defined(WIN32)
		WSACleanup();
#endif /* defined(WIN32) */
		::testing::Test::TearDown();
	}

	char *fileName;
	intptr_t fileDescriptor;

public:
	PortFileTest()
		: ::testing::Test()
		, fileName(NULL)
		, fileDescriptor(-1)
	{
	}
};

/**
 * @internal
 * Write to a file
 * @param[in] portLibrary The port library under test
 * @param[in] fd File descriptor to write to
 * @param[in] format Format string to write
 * @param[in] ... Arguments for format
 *
 * @todo Now that omrfile_printf exists, we should probably use that instead.
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

/**
 * Verify port library file sub-allocator.
 *
 * Ensure the port library is properly setup to run file operations.
 */
TEST(PortInitializationTest, File)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	/* Some operations are always present, even if the port library does not
	 * support file systems.  Verify these first
	 */

	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibary, it is not re-entrant safe
	 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_startup);

	/* Not tested, implementation dependent.  No known functionality */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_shutdown);

	/* omrfile_test1 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_write);

	/* omrfile_test2 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_write_text);

	/* omrfile_test3 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_vprintf);

	/* omrfile_test17 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_printf);

	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_blockingasync_close);
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_blockingasync_open);
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_blockingasync_lock_bytes);
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_blockingasync_unlock_bytes);
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_blockingasync_read);
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_blockingasync_write);
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_blockingasync_set_length);
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_blockingasync_flength);
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_blockingasync_shutdown);
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_blockingasync_startup);

	/* Verify that the file function pointers are non NULL */

	/* omrfile_test5, omrfile_test6 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_open);

	/* omrfile_test5, omrfile_test6 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_close);

	/* omrfile_test10 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_seek);

	/* omrfile_test7, omrfile_test8 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_read);

	/* omrfile_test11, omrfile_test12 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_unlink);

	/* omrfile_test4 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_attr);

	/* omrfile_test9 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_lastmod);

	/* omrfile_test9 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_length);

	/* omrfile_test9 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_set_length);

	/* omrfile_test15 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_sync);

	/* omrfile_test25 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_stat);

	/* functions  available with standard configuration */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_read_text); /* TODO omrfiletext.c */

	/* omrfile_test11 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_mkdir);

	/* omrfile_test13 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_move);

	/* omrfile_test11 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_unlinkdir);

	/* omrfile_test14 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_findfirst);

	/* omrfile_test14 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_findnext);

	/* omrfile_test14 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_findclose);

	/* omrfile_test16 */
	OMRTEST_EXPECT_NOT_NULL(OMRPORTLIB->file_error_message);
}

/**
 * Verify port file read and write.
 */
TEST_F(PortFileTest, WriteRead)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	char expectedResult[] = "Try a number - 1Try a string - abcTry a mixture - 1 abc 2";
	char readBuf[255];
	char *readBufPtr;

	fileDescriptor = omrfile_open(fileName, EsOpenCreate | EsOpenWrite, 0666);
	OMRTEST_ASSERT_TRUE((-1 != fileDescriptor), "omrfile_open \"" << fileName << "\" failed\n");

	fileWrite(OMRPORTLIB, fileDescriptor, "Try a number - %d", 1);

	fileWrite(OMRPORTLIB, fileDescriptor, "Try a string - %s", "abc");
	fileWrite(OMRPORTLIB, fileDescriptor, "Try a mixture - %d %s %d", 1, "abc", 2);

	/* having hopefully shoved this stuff onto file we need to read it back and check its as expected! */
	int32_t rc = omrfile_close(fileDescriptor);
	OMRTEST_ASSERT_TRUE((0 == rc), "omrfile_close() returned " << rc << " expected " << "0\n");

	fileDescriptor = omrfile_open(fileName, EsOpenRead, 0444);
	OMRTEST_ASSERT_TRUE((-1 != fileDescriptor), "omrfile_open \"" << fileName << "\" failed\n");

	readBufPtr = omrfile_read_text(fileDescriptor, readBuf, sizeof(readBuf));
	OMRTEST_ASSERT_TRUE((NULL != readBufPtr), "omrfile_read_text() returned NULL\n");

	OMRTEST_ASSERT_TRUE((strlen(readBufPtr) == strlen(expectedResult)), "omrfile_read_text() returned " << strlen(readBufPtr) << " expected " << strlen(expectedResult) << "\n");

	/* omrfile_read_text(): translation from IBM1047 to ASCII is done on zOS */
	OMRTEST_ASSERT_TRUE((!strcmp(readBufPtr, expectedResult)), "omrfile_read_text() returned \"" << readBufPtr << "\" expected \"" << expectedResult << "\"\n");
}

/**
 * Verify port file system.
 *
 * Verify
 * @ref omrfile.c::omrfile_read "omrfile_read()" and
 * @ref omrfile.c::omrfile_write "omrfile_write()"
 * append operations.
 *
 * This test creates a file, writes ABCDE to it then closes it - reopens it for append and
 * writes FGHIJ to it, fileseeks to start of file and reads 10 chars and hopefully this matches
 * ABCDEFGHIJ ....
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */

TEST_F(PortFileTest, WriteAppendRead)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());

	intptr_t expectedReturnValue;
	intptr_t rc;
	char inputLine1[] = "ABCDE";
	char inputLine2[] = "FGHIJ";
	char outputLine[] = "0123456789";
	char expectedOutput[] = "ABCDEFGHIJ";

	expectedReturnValue = sizeof(inputLine1) - 1; /* don't want the final 0x00 */

	/*lets try opening a "new" file - if file exists then should still work */
	fileDescriptor = omrfile_open(fileName, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	OMRTEST_ASSERT_TRUE((-1 != fileDescriptor), "omrfile_open \"" << fileName << "\" failed\n");

	/* now lets write to it */
	rc = omrfile_write(fileDescriptor, inputLine1, expectedReturnValue);
	OMRTEST_ASSERT_TRUE((rc == expectedReturnValue), "omrfile_write() returned " << rc << " expected " << expectedReturnValue << "\n");

	rc = omrfile_close(fileDescriptor);
	OMRTEST_ASSERT_TRUE((0 == rc), "omrfile_close() failed\n");

	fileDescriptor = omrfile_open(fileName, EsOpenWrite | EsOpenAppend, 0666);
	OMRTEST_ASSERT_TRUE((-1 != fileDescriptor), "omrfile_open \"" << fileName << "\" failed\n");

	/* now lets write to it */
	expectedReturnValue = sizeof(inputLine2) - 1; /* don't want the final 0x00 */
	rc = omrfile_write(fileDescriptor, inputLine2, expectedReturnValue);
	OMRTEST_ASSERT_TRUE((rc == expectedReturnValue), "omrfile_write() returned " << rc << ", expected " << expectedReturnValue << "\n");

	rc = omrfile_close(fileDescriptor);
	OMRTEST_ASSERT_TRUE((0 == rc), "omrfile_close() failed\n");

	fileDescriptor = omrfile_open(fileName, EsOpenRead, 0444);
	OMRTEST_ASSERT_TRUE((-1 != fileDescriptor), "omrfile_open \"" << fileName << "\" failed\n");

	/* lets read back what we wrote */
	expectedReturnValue = 10;
	rc = omrfile_read(fileDescriptor, outputLine, expectedReturnValue);

	OMRTEST_ASSERT_TRUE((rc == expectedReturnValue), "omrfile_read() returned " << rc << ", expected " << expectedReturnValue << "\n");

	OMRTEST_ASSERT_TRUE((!strcmp(expectedOutput, outputLine)), "Read back data \"" << outputLine << "\" not matching expected data \"" << expectedOutput << "\"\n");

	rc = omrfile_close(fileDescriptor);
	OMRTEST_ASSERT_TRUE((0 == rc), "omrfile_close() failed\n");
}
