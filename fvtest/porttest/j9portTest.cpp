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
 * $RCSfile: omrportTest.c,v $
 * $Revision: 1.42 $
 * $Date: 2012-11-23 21:11:32 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library management operations.
 *
 * Exercise the API for port library management.  These functions
 * can be found in the file @ref omrport.c  The majority of these functions are exported
 * by the port library and are not in the actual port library table, however there
 * are a few.
 *
 * @note port library maintenace operations are not optional in the port library table.
 */
#include <stdlib.h>
#include <string.h>

#include "testHelpers.hpp"
#include "omrport.h"

/**
 * @internal
 * Some platforms can't handle large objects on their stack, so use a global
 * version of the portlibrary for testing.
 */
OMRPortLibrary portLibraryToTest;

/**
 * @internal
 * Some platforms can't handle large objects on their stack, so use a global
 * version of a large buffer representing memory for a port library.
 * @{
 */
#define bigBufferSize (2*sizeof(OMRPortLibrary))
char bigBuffer[bigBufferSize];
/** @} */

/**
 * @internal
 * Verify OMRPortLibrary structure.
 *
 * Given a port library structure, verify that it is correctly setup.
 *
 * @param[in] portLibrary the port library to verify
 * @param[in] testName test requesting creation
 * @param[in] version the version of port library to verify
 * @param[in] size the expected size
 */
static void
verifyCreateTableContents(struct OMRPortLibrary *portLibrary, const char *testName, uintptr_t size)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	omrport_getSize();

	/* portGlobals should not be allocated */
	if (NULL != OMRPORTLIB->portGlobals) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_create_library() portGlobals pointer is not NULL\n");
	}

	/* Not self allocated */
	if (NULL != OMRPORTLIB->self_handle) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_create_library() self_handle pointer is not NULL\n");
	}

	/* Verify the pointers look correct, must skip version and portGlobals portion information */

	/* reach into the port library for the various values of interest */
	if (NULL == OMRPORTLIB->tty_printf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_create_library() tty_printf pointer is NULL\n");
	}
}

/**
 * @internal
 * Verify port library startup.
 *
 * A known failure reason for port library startup
 */
static const int32_t failMemStartup = -100;

/**
 * @internal
 * Verify port library startup.
 *
 * A startup function that can be put into a port library to cause failure
 * at startup.
 *
 * @param[in] portLibrary the port library
 * @param[in] portGlobalSize size of portGlobal structure
 *
 * @return negative error code
 */
static int32_t
fake_mem_startup(struct OMRPortLibrary *portLibrary, uintptr_t portGlobalSize)
{
	return failMemStartup;
}

/**
 * @internal
 * Verify port library startup.
 *
 * A startup function that can be put into a port library to cause failure
 * at startup.
 *
 * @param[in] portLibrary the port library
 *
 * @return negative error code
 */
static int32_t
fake_time_startup(struct OMRPortLibrary *portLibrary)
{
	return failMemStartup;
}

/**
 * Verify port library management.
 *
 * Ensure the port library is properly setup to run port library maintenance tests.
 *
 * Most operations being tested are not stored in the port library table
 * as they are required for port library allocation, startup and shutdown.
 * There are a couple to look up.
 */
TEST(PortTest, port_test0)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrport_test0";

	reportTestEntry(OMRPORTLIB, testName);

	/* Verify that the omrport function pointers are non NULL */

	/* omrport_test7 */
	if (NULL == OMRPORTLIB->port_shutdown_library) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->port_shutdown_library is NULL\n");
	}

	/* omrport_test9 */
	if (NULL == OMRPORTLIB->port_isFunctionOverridden) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->port_isFunctionOverridden is NULL\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library management.
 *
 * Applications wishing to self allocate the port library are required to provide
 * a size for the port library to create.  The size of any port library version can
 * be obtained via the exported function
 * @ref omrport.c::omrport_getSize "omrport_getSize()"
 */
TEST(PortTest, port_test1)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrport_test1";
	uintptr_t expectedSize;
	uintptr_t actualSize;

	/* First all supported versions */
	expectedSize = sizeof(OMRPortLibrary);

	/* Pass, exact match all fields */
	actualSize = omrport_getSize();
	if (actualSize != expectedSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_getSize() returned %zu expected %zu\n", actualSize, expectedSize);
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library management.
 *
 * Prior to starting a port library the initial values for the function pointers
 * in the table need to be initialized.  The ability to initialize the table with
 * default values prior to starting the port library allows applications to override
 * functionality.  For example an application may with to control all memory management,
 * so overriding the appropriate functions is required prior to any memory being
 * allocated.  Creation of the port library table is done via the function
 * @ref omrport.c::omrport_create_library "omrport_create_library()"
 *
 * @see omrport.c::omrport_startup_library "omrport_startup_library()"
 * @see omrport.c::omrport_init_library "omrport_init_library()"
 */
TEST(PortTest, port_test2)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrport_test2";

	OMRPortLibrary *fakePtr = (OMRPortLibrary *)bigBuffer;
	char eyeCatcher;
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	/* Pass, buffer bigger than required */
	eyeCatcher = '1';
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = omrport_create_library(fakePtr, 2 * sizeof(OMRPortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_create_library() returned %d expected %d\n", rc);
	}
	verifyCreateTableContents(fakePtr, testName, sizeof(OMRPortLibrary));

	/* Pass, should create a library equal to that currently running */
	eyeCatcher = '2';
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = omrport_create_library(fakePtr, sizeof(OMRPortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_create_library() returned %d expected 0\n", rc);

	}

	/* Fail, buffer 1 byte too small */
	eyeCatcher = '3';
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = omrport_create_library(fakePtr, sizeof(OMRPortLibrary) - 1);
	if (OMRPORT_ERROR_INIT_OMR_WRONG_SIZE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_create_library() returned %d expected %d\n", rc, OMRPORT_ERROR_INIT_OMR_WRONG_SIZE);
	}
	/* Ensure we didn't write anything, being lazy only checking first byte */
	if (eyeCatcher != bigBuffer[0]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_create_library() trampled memory expected \"%c\" found \"%c\"\n", eyeCatcher, bigBuffer[0]);
	}

	/* Fail, buffer size way too small */
	eyeCatcher = '4';
	memset(fakePtr, eyeCatcher, bigBufferSize);
	rc = omrport_create_library(fakePtr, 20);
	if (OMRPORT_ERROR_INIT_OMR_WRONG_SIZE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_create_library() returned %d expected %d\n", rc, OMRPORT_ERROR_INIT_OMR_WRONG_SIZE);
	}
	/* Ensure we didn't write anything, being lazy only checking first byte */
	if (eyeCatcher != bigBuffer[0]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_create_library() trampled memory expected \"%c\" found \"%c\"\n", eyeCatcher, bigBuffer[0]);
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library management.
 *
 * The second stage in starting a port library.  Once a port library table has
 * been created via
 * @ref omrport.c::omrport_create_library "omrport_create_library()"
 * it is started via the exported function
 * @ref omrport.c::omrport_startup_library "omrport_startup_library()".
 *
 * @see omrport.c::omrport_create_library "omrport_create_library()"
 * @see omrport.c::omrport_init_library "omrport_init_library()"
 */
TEST(PortTest, port_test3)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrport_test3";

	OMRPortLibrary *fakePtr = &portLibraryToTest;
	int32_t rc;
	omrthread_t attachedThread = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	if (0 != omrthread_attach_ex(&attachedThread, J9THREAD_ATTR_DEFAULT)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to attach to omrthread\n");
		goto exit;
	}

	/* create a library and ensure the port globals is non NULL, what else can we do? */
	memset(&portLibraryToTest, '0', sizeof(OMRPortLibrary));
	rc = omrport_create_library(fakePtr, sizeof(OMRPortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_create_library() returned %d expected 0\n", rc);
		goto exit;
	}

	/* check the rc */
	rc = omrport_startup_library(fakePtr);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_startup_library() returned %d expected 0\n", rc);
	}

	/* check portGlobals were allocated */
	if (NULL == fakePtr->portGlobals) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_startup_library() portGlobals not allocated\n");
	}

	if (NULL != fakePtr->self_handle) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_startup_library() self_handle pointer not NULL\n");
	}

	/* Clean this port library up */
	portLibraryToTest.port_shutdown_library(fakePtr);

	/* Do it again, but before starting override memory (first startup function) and
	 * ensure everything is ok
	 */
	memset(&portLibraryToTest, '0', sizeof(OMRPortLibrary));
	rc = omrport_create_library(fakePtr, sizeof(OMRPortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_create_library() returned %d expected 0\n", rc);
		goto exit;
	}

	/* override time_startup */
	fakePtr->time_startup = fake_time_startup;
	rc = omrport_startup_library(fakePtr);
	if (failMemStartup != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_startup_library() returned %d expected %d\n", rc, failMemStartup);
	}

exit:
	portLibraryToTest.port_shutdown_library(fakePtr);
	if (NULL != attachedThread) {
		omrthread_detach(attachedThread);
	}
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library management.
 *
 * The majority of applications will not wish to override any functionality
 * prior to use of the port library.  The exported function
 * @ref omrport.c::omrport_init_library "omrport_init_library()"
 * creates and starts the port library.
 *
 * @see omrport.c::omrport_create_library "omrport_create_library()"
 * @see omrport.c::omrport_startup_library "omrport_startup_library()"
 */
TEST(PortTest, port_test4)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrport_test4";

	reportTestEntry(OMRPORTLIB, testName);

	/* TODO copy test3 here once it compiles */

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library management.
 *
 * The port library requires a region of memory for it's function table pointer.
 * This table must not be declared on a stack that is torn down prior to the application
 * running to completion and tearing down the port library.  It can either be
 * created as a global variable to the program, or if a running port library is
 * present it can be allocated on behalf of the application.  Self allocating
 * port libraries use the exported function
 * @ref omrport.c::omrport_allocate_library "omrport_allocate_library()".
 * This function allocates, initializes and starts the port library.
 * The port library is responsible for deallocation of this memory
 * as part of it's shutdown.
 */
TEST(PortTest, port_test5)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrport_test5";

	OMRPortLibrary *fakePortLibrary = NULL;
	int32_t rc;
	omrthread_t attachedThread = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	if (0 != omrthread_attach_ex(&attachedThread, J9THREAD_ATTR_DEFAULT)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to attach to omrthread\n");
		goto exit;
	}

	/* Pass */
	fakePortLibrary = NULL;
	rc = omrport_allocate_library(&fakePortLibrary);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_allocate_library() returned %d expected 0\n", rc);
		goto exit;
	}

	/* Verify we have a pointer */
	if (NULL == fakePortLibrary) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_allocate_library() returned NULL pointer\n");
		goto exit;
	}

	/* Verify not started */
	if (NULL != fakePortLibrary->portGlobals) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_allocate_library() portGlobals pointer is not NULL\n");
	}

	/* Verify self allocated */
	if (NULL == fakePortLibrary->self_handle) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_allocate_library() self_handle pointer is NULL\n");
	}

	/* Verify it will start */
	rc = omrport_startup_library(fakePortLibrary);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_startup_library() returned %d expected 0\n", rc);
	}

	/* Take this port library down */
	fakePortLibrary->port_shutdown_library(fakePortLibrary);

	/* Try again, but force failure during startup by over riding one of the startup functions */
	fakePortLibrary = NULL;
	rc = omrport_allocate_library(&fakePortLibrary);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_allocate_library() returned %d expected 0\n", rc);
		goto exit;
	}

	/* Override omrmem_startup */
	fakePortLibrary->mem_startup = fake_mem_startup;
	rc = omrport_startup_library(fakePortLibrary);
	if (failMemStartup != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_startup_library() returned %d expected %d\n", rc, failMemStartup);
	}

	/* No way to tell if it freed the memory properly.  The pointer we have to the port library is
	 * not updated as part of omrport_startup_library(), so we are now pointing at dead memory.
	 *
	 * NULL our pointer to exit can clean-up if required
	 */
	fakePortLibrary = NULL;

exit:
	if (NULL != fakePortLibrary) {
		fakePortLibrary->port_shutdown_library(fakePortLibrary);
	}
	if (NULL != attachedThread) {
		omrthread_detach(attachedThread);
	}
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library management.
 *
 * The port library allocates resources as part of it's normal running.  Thus
 * prior to terminating the application the port library must free all it's
 * resource via the port library table function
 * @ref omrport.c::omrport_shutdown_library "omrport_shutdown_library()"
 *
 * @note self allocated port libraries de-allocate their memory as part of this
 * function.
 */
TEST(PortTest, port_test6)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrport_test6";

	OMRPortLibrary *fakePtr;
	int32_t rc;
	omrthread_t attachedThread = NULL;

	reportTestEntry(OMRPORTLIB, testName);

	if (0 != omrthread_attach_ex(&attachedThread, J9THREAD_ATTR_DEFAULT)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to attach to omrthread\n");
		goto exit;
	}

	memset(&portLibraryToTest, '0', sizeof(OMRPortLibrary));
	fakePtr = &portLibraryToTest;
	rc = omrport_init_library(fakePtr, sizeof(OMRPortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_init_library() returned %d expected 0\n", rc);
	}
	/* Shut it down */
	portLibraryToTest.port_shutdown_library(fakePtr);
	omrthread_detach(attachedThread);

	/* Global pointer should be NULL */
	if (NULL != fakePtr->portGlobals) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "port_shutdown_library() portGlobal pointer is not NULL\n");
	}

	/* Shutdown again, should be ok, nothing to check ...
	 * TODO support this?
	portLibraryToTest.port_shutdown_library(fakePtr);
	 */

	/* Let's do it all over again, this time self allocated
	 * Note a self allocated library does not startup, so there are no
	 * port globals etc.  Verifies the shutdown routines are safe
	 */
	fakePtr = NULL;
	rc = omrport_allocate_library(&fakePtr);
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_allocate_library() returned %d expected 0\n", rc);
		goto exit;
	}
	/*	TODO library not started, fair to shut it down ?
	fakePtr->port_shutdown_library(fakePtr);
	*/
	fakePtr = NULL;

exit:
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library management.
 *
 * The JIT needs to verify that the function it wants to optimize is what
 * it expects, that is an application has not provided it's own implementation.
 * The port library table function
 * @ref omrport.c::omrport_isFunctionOverridden "omrport_isFunctionOverridden"
 * is used for this purpose.
 */
TEST(PortTest, port_test7)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrport_test7";

	OMRPortLibrary *fakePtr = &portLibraryToTest;
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	memset(&portLibraryToTest, '0', sizeof(OMRPortLibrary));
	rc = omrport_init_library(&portLibraryToTest, sizeof(OMRPortLibrary));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_init_library() returned %d expected 0\n", rc);
	}

	/* override a couple of function */
	fakePtr->mem_startup = fake_mem_startup;
	fakePtr->mem_shutdown = NULL;
	fakePtr->time_hires_clock = NULL;
	fakePtr->time_startup = fakePtr->tty_startup;

	rc = fakePtr->port_isFunctionOverridden(fakePtr, offsetof(OMRPortLibrary, mem_startup));
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_isFunctionOverridden returned %d expected 1\n", rc);
	}

	rc = fakePtr->port_isFunctionOverridden(fakePtr, offsetof(OMRPortLibrary, mem_shutdown));
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_isFunctionOverridden returned %d expected 1\n", rc);
	}

	rc = fakePtr->port_isFunctionOverridden(fakePtr, offsetof(OMRPortLibrary, time_hires_clock));
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_isFunctionOverridden returned %d expected 1\n", rc);
	}

	rc = fakePtr->port_isFunctionOverridden(fakePtr, offsetof(OMRPortLibrary, time_startup));
	if (1 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_isFunctionOverridden returned %d expected 1\n", rc);
	}

	rc = fakePtr->port_isFunctionOverridden(fakePtr, offsetof(OMRPortLibrary, time_hires_frequency));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_isFunctionOverridden returned %d expected 0\n", rc);
	}

	rc = fakePtr->port_isFunctionOverridden(fakePtr, offsetof(OMRPortLibrary, tty_printf));
	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrport_isFunctionOverridden returned %d expected 0\n", rc);
	}

	reportTestExit(OMRPORTLIB, testName);
}

