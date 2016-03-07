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
 * $RCSfile: omrerrorTest.c,v $
 * $Revision: 1.32 $
 * $Date: 2011-12-30 14:15:00 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library error handling operations.
 *
 * Exercise the API for port library error handling operations.  These functions
 * can be found in the file @ref omrerror.c
 *
 * @note port library error handling operations are not optional in the port library table.
 *
 * @TODO make this multi-threaded.
 *
 */
#include <stdlib.h>
#include <string.h>

#include "testHelpers.hpp"
#include "omrport.h"

/**
 * Verify port library error handling operations.
 *
 * Ensure the library table is properly setup to run error handling tests.
 */
TEST(PortErrorTest, error_test0)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrerror_test0";

	reportTestEntry(OMRPORTLIB, testName);

	/* Verify that the error handling function pointers are non NULL */

	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibary, it is not re-entrant safe
	 */
	if (NULL == OMRPORTLIB->error_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_startup is NULL\n");
	}

	/* Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORTLIB->error_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_shutdown is NULL\n");
	}

	/* omrerror_test1 */
	if (NULL == OMRPORTLIB->error_last_error_number) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_last_error_number is NULL\n");
	}

	/* omrerror_test2 */
	if (NULL == OMRPORTLIB->error_last_error_message) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_last_error_message is NULL\n");
	}

	/* omrerror_test1 */
	if (NULL == OMRPORTLIB->error_set_last_error) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_set_last_error is NULL\n");
	}

	/* omrerror_test2 */
	if (NULL == OMRPORTLIB->error_set_last_error_with_message) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->error_set_last_error_with_message is NULL\n");
	}
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library error handling operations.
 *
 * Error codes are stored in per thread buffers so errors reported by one thread
 * do not effect those reported by a second.  Errors stored via the helper
 * function @ref omrerror.c::omrerror_set_last_error "omrerror_set_last_error()" are recorded in the per thread
 * buffers without an error message.
 * Verify the @ref omrerror_last_error_number "omrerror_last_error_number()" returns
 * the correct portable error code.
 */
TEST(PortErrorTest, error_test1)
{

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrerror_test1";

	int32_t errorCode;

	reportTestEntry(OMRPORTLIB, testName);

	/* Delete the ptBuffers */
	omrport_tls_free();

	/* In theory there is now nothing stored, if we really did free the buffers.
	 * Guess it is time to find out
	 */
	errorCode = omrerror_last_error_number();
	if (0 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_number() returned %d expected %d\n", errorCode, 0);
	}

	/* Set an error code, verify it is what we get back */
	errorCode = omrerror_set_last_error(100, 200);
	if (200 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_set_last_error() returned %d expected %d\n", errorCode, 200);
	}
	errorCode = omrerror_last_error_number();
	if (200 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_number() returned %d expected %d\n", errorCode, 200);
	}

	/* Again with negative values */
	errorCode = omrerror_set_last_error(100, -200);
	if (-200 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_set_last_error() returned %d expected %d\n", errorCode, 200);
	}
	errorCode = omrerror_last_error_number();
	if (-200 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_number() returned %d expected %d\n", errorCode, 200);
	}

	/* Delete the ptBuffers, verify no error is stored */
	omrport_tls_free();
	errorCode = omrerror_last_error_number();
	if (0 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_number() returned %d expected %d\n", errorCode, 0);
	}
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library error handling operations.
 *
 * Error codes are stored in per thread buffers so errors reported by one thread
 * do not effect those reported by a second.  Errors stored via the helper
 * function @ref omrerror.c::omrerror_set_last_error_with_message "omrerror_set_last_error_with_message()"
 * are recorded in the per thread buffers without an error message.
 * Verify the @ref omrerror_last_error_message "omrerror_last_error_message()" returns
 * the correct message.
 */
TEST(PortErrorTest, error_test2)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrerror_test2";

	const char *message;
	const char *knownMessage = "This is a test";
	const char *knownMessage2 = "This is also a test";
	int32_t errorCode;

	reportTestEntry(OMRPORTLIB, testName);

	/* Delete the ptBuffers */
	omrport_tls_free();

	/* In theory there is now nothing stored, if we really did free the buffers.
	 * Guess it is time to find out
	 */
	message = omrerror_last_error_message();
	if (0 != strlen(message)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_message() returned message of length %d expected %d\n", strlen(message), 1);
	}

	/* Set an error message, verify it is what we get back */
	errorCode = omrerror_set_last_error_with_message(200, knownMessage);
	message = omrerror_last_error_message();
	if (200 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_set_last_error_with_message() returned %d expected %d\n", errorCode, 200);
	}
	if (strlen(message) != strlen(knownMessage)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_message() returned length %d expected %d\n", strlen(message), strlen(knownMessage));
	}
	if (0 != memcmp(message, knownMessage, strlen(knownMessage))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_message() returned \"%s\" expected \"%s\"\n", message, knownMessage);
	}

	/* Again, different message */
	errorCode = omrerror_set_last_error_with_message(100, knownMessage2);
	message = omrerror_last_error_message();
	if (100 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_set_last_error_with_message() returned %d expected %d\n", errorCode, 100);
	}
	if (strlen(message) != strlen(knownMessage2)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_message() returned length %d expected %d\n", strlen(message), strlen(knownMessage2));
	}
	if (0 != memcmp(message, knownMessage2, strlen(knownMessage2))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_message() returned \"%s\" expected \"%s\"\n", message, knownMessage2);
	}

	/* A null message, valid test?*/
#if 0
	errorCode = omrerror_set_last_error_with_message(-100, NULL);
	message = omrerror_last_error_message();
	if (-100 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_message() returned %d expected %d\n", errorCode, -100);
	}
	if (NULL != message) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_message() returned \"%s\" expected NULL\n", message);
	}
#endif

	omrport_tls_free();
	errorCode = omrerror_set_last_error_with_message(-300, knownMessage);
	message = omrerror_last_error_message();
	if (-300 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_number() returned %d expected %d\n", errorCode, -300);
	}
	if (strlen(message) != strlen(knownMessage)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_message() returned length %d expected %d\n", strlen(message), strlen(knownMessage));
	}
	if (0 != memcmp(message, knownMessage, strlen(knownMessage))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_message() returned \"%s\" expected \"%s\"\n", message, knownMessage);
	}

	/* Delete the ptBuffers, verify no error is stored */
	omrport_tls_free();
	errorCode = omrerror_last_error_number();
	if (0 != errorCode) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrerror_last_error_number() returned %d expected %d\n", errorCode, 0);
	}
	reportTestExit(OMRPORTLIB, testName);
}
