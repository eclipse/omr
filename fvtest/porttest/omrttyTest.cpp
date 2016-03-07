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
 * $RCSfile: omrttyTest.c,v $
 * $Revision: 1.31 $
 * $Date: 2011-12-30 14:15:00 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify TTY operations
 *
 * Exercise the API for port library functions in @ref omrtty.c
 *
 * @note TTY operations are not optional in the port library table.
 *
 * @internal
 * @note TTY operations are mainly helper functions for @ref omrfile.c::omrfile_vprintf "omrfile_vprintf" and
 * @ref omrfileText.c::omrfile_write_file "omrfile_write_file()".  Verify that the TTY operations call the appropriate functions.
 *
 */
#include <string.h>
#include <stdio.h>

#include "testHelpers.hpp"
#include "omrport.h"

#define J9TTY_STARTUP_PRIVATE_RETURN_VALUE ((int32_t) 0xDEADBEEF) /** <@internal A known return for @ref fake_omrtty_startup */

/**
 * @internal
 * @typdef
 * Function prototype for verifying helpers call @ref omrfile.c::omrfile_vprintf "omrfile_vprintf()"
 */
typedef void (*J9FILE_VPRINTF_FUNC)(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *format, va_list args);

/**
 * @internal
 * @typdef
 * Function prototype for verifying helpers call @ref omrfileText.c::omrfile_write_text "omrfile_write_text()"
 */
typedef intptr_t (*J9FILE_WRITE_TEXT_FUNC)(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *buf, intptr_t nbytes);

/**
 * @internal
 * @typdef
 * Function prototype for verifying helpers call @ref omrtty.c::omrtty_vprintf "omrtty_vprintf()"
 */
typedef void (*J9TTY_VPRINTF_FUNC)(struct OMRPortLibrary *portLibrary, const char *format, va_list args);

/**
 * @internal
 * @typdef
 * Function prototype for verifying helpers call @ref omrtty.c::omrtty_vprintf "omrtty_vprintf()"
 */
typedef void (*J9TTY_ERR_VPRINTF_FUNC)(struct OMRPortLibrary *portLibrary, const char *format, va_list args);

/**
 * @internal
 * @typdef
 * Function prototype for verifying helpers call @ref omrfile.c::omrfile_write_text" omrfile_write_text()"
 */
typedef int32_t (*J9TTY_STARTUP_FUNC)(struct OMRPortLibrary *portLibrary);

/**
 * @internal
 * Verify helpers functions call correct functions.
 *
 * Override @ref j9tt.c::omrtty_startup "omrtty_startup()" and return a known value.
 *
 * @param[in] portLibrary The port library
 *
 * @return J9TTY_STARTUP_PRIVATE_RETURN_VALUE.
 */
static int32_t
fake_omrtty_startup(struct OMRPortLibrary *portLibrary)
{
	return J9TTY_STARTUP_PRIVATE_RETURN_VALUE;
}

/**
 * @internal
 * Verify helpers functions call correct functions.
 *
 * Override @ref omrfile.c::omrfile_vprintf  "omrfile_vprintf()" and return a known value to verify any helper
 * functions that  should be calling this function.
 *
 * @ref omrfile.c::omrfile_vprintf  "omrfile_vprintf()" does not return a value, the caller needs a way to know this overriding
 * function has been called.  As a result this function simply replaces the port library function table slot tty_startup with
 * a second overriding function.  The caller then calls @ref omrtty.c::omrtty_startup "omrtty_startup()" to verify they have
 * indeed overridden @ref omrfile.c::omrfile_vprintf  "omrfile_vprintf()" .
 *
 * @param[in] portLibrary The port library
 * @param[in] fd File descriptor to write.
 * @param[in] format The format String.
 * @param[in] args Variable argument list.
 *
 * @note The caller is reponsible for restoring the tty_startup slot in the port library function table.
 */
void
fake_omrfile_vprintf(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *format, va_list args)
{
	/* No need to save the real value, it is not restored here */
	portLibrary->tty_startup = fake_omrtty_startup;
}

/**
 * @internal
 * Verify helpers functions call correct functions.
 *
 * Override @ref omrtty.c::omrtty_vprintf  "omrtty_vprintf()" and return a known value to verify any helper
 * functions that  should be calling this function.
 *
 * @ref omrtty.c::omrtty_vprintf  "omrtty_vprintf()" does not return a value, the caller needs a way to know this overriding
 * function has been called.  As a result this function simply replaces the port library function table slot tty_startup with
 * a second overriding function.  The caller then calls @ref omrtty.c::omrtty_startup "omrtty_startup()" to verify they have
 * indeed overridden @ref omrtty.c::omrtty_vprintf  "omrtty_vprintf()" .
 *
 * @param[in] portLibrary The port library.
 * @param[in] format The format String.
 * @param[in] args Variable argument list..
 *
 * @note The caller is reponsible for restoring the tty_startup slot in the port library function table.
 */
void
fake_omrtty_vprintf(struct OMRPortLibrary *portLibrary, const char *format, va_list args)
{
	/* No need to save the real value, it is not restored here */
	portLibrary->tty_startup = fake_omrtty_startup;
}

/**
 * @internal
 * Verify helpers functions call correct functions.
 *
 * Override @ref omrtty.c::omrtty_err_vprintf  "omrtty_err_vprintf()" and return a known value to verify any helper
 * functions that  should be calling this function.
 *
 * @ref omrtty.c::omrtty_err_vprintf  "omrtty_err_vprintf()" does not return a value, the caller needs a way to know this overriding
 * function has been called.  As a result this function simply replaces the port library function table slot tty_startup with
 * a second overriding function.  The caller then calls @ref omrtty.c::omrtty_startup "omrtty_startup()" to verify they have
 * indeed overridden @ref omrtty.c::omrtty_err_vprintf  "omrtty_err_vprintf()" .
 *
 * @param[in] portLibrary The port library.
 * @param[in] format The format String.
 * @param[in] args Variable argument list..
 *
 * @note The caller is reponsible for restoring the tty_startup slot in the port library function table.
 */
void
fake_omrtty_err_vprintf(struct OMRPortLibrary *portLibrary, const char *format, va_list args)
{
	/* No need to save the real value, it is not restored here */
	portLibrary->tty_startup = fake_omrtty_startup;
}

/**
 * Verify port library TTY operations.
 *
 * Ensure the library table is properly setup to run TTY tests.
 */
TEST(PortTTyTest, tty_test0)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtty_test0";

	reportTestEntry(OMRPORTLIB, testName);

	/* The TTY functions should be implemented in terms of other functions.  Ensure those function
	 * pointers are correctly setup.  The API for TTY functions clearly state these relationships.
	 * @TODO make this true
	 */
	if (NULL == OMRPORTLIB->file_vprintf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_vprintf is NULL\n");
	}

	if (NULL == OMRPORTLIB->file_write_text) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->file_write_text is NULL\n");
	}

	/* Verify that the TTY function pointers are non NULL */

	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibary, it is not re-entrant safe
	 */
	if (NULL == OMRPORTLIB->tty_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_startup is NULL\n");
	}

	/* Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORTLIB->tty_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_shutdown is NULL\n");
	}

	/* omrtty_test1 */
	if (NULL == OMRPORTLIB->tty_printf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_printf is NULL\n");
	}

	/* omrtty_test2 */
	if (NULL == OMRPORTLIB->tty_vprintf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_vprintf is NULL\n");
	}

	/* omrtty_test3 */
	if (NULL == OMRPORTLIB->tty_get_chars) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_get_chars is NULL\n");
	}

	/* omrtty_test4 */
	if (NULL == OMRPORTLIB->tty_err_printf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_err_printf is NULL\n");
	}

	/* omrtty_test5 */
	if (NULL == OMRPORTLIB->tty_err_vprintf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_err_vprintf is NULL\n");
	}

	/* omrtty_test6 */
	if (NULL == OMRPORTLIB->tty_available) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->tty_available is NULL\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library TTY operations.
 *
 * @ref omrtty.c::omrtty_printf "omrtty_printf()" is a helper function for
 * @ref omrtty.c::omrtty_vprintf "omrtty_vprintf()".  Ensure that relationship is properly set up.
 */
TEST(PortTTyTest, tty_test1)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtty_test1";

	J9TTY_STARTUP_FUNC real_tty_startup;
	J9TTY_VPRINTF_FUNC real_tty_vprintf;
	int32_t  rc;

	reportTestEntry(OMRPORTLIB, testName);

	/* Save the real function pointers from the port library table */
	real_tty_vprintf = OMRPORTLIB->tty_vprintf;
	real_tty_startup = OMRPORTLIB->tty_startup;

	/* Override the tty_vprintf function only, it will replace tty_startup since tty_vprintf
	 * won't return a value for verification
	 */
	OMRPORTLIB->tty_vprintf = fake_omrtty_vprintf;

	/* Verify tty_startup was correctly overridden, the parameters passed should not matter */
	omrtty_printf("You should not see this\n");
	rc = OMRPORTLIB->tty_startup(OMRPORTLIB);

	/* Restore the function pointers */
	OMRPORTLIB->tty_vprintf = real_tty_vprintf;
	OMRPORTLIB->tty_startup = real_tty_startup ;

	if (J9TTY_STARTUP_PRIVATE_RETURN_VALUE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtty_printf() does not call omrtty_vprintf()\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * @internal
 * Wrapper to call omrtty_vprintf
 */
static void
omrtty_vprintf_wrapper(struct OMRPortLibrary *portLibrary, const char *format, ...)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	va_list args;

	va_start(args, format);
	omrtty_vprintf(format, args);
	va_end(args);
}

/**
 * Verify port library TTY operations.
 *
 * @ref omrtty.c::omrtty_vprintf "omrtty_vprintf()" is a helper function for
 * @ref omrfile.c::omrfile_vprintf "omrfile_vprintf()".  Ensure that relationship is properly set up.
 */
TEST(PortTTyTest, tty_test2)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtty_test2";

	J9TTY_STARTUP_FUNC real_tty_startup;
	J9FILE_VPRINTF_FUNC real_file_vprintf;
	const char *format = "You should not see this %d %d %d\n";
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	/* Save the real function pointers from the port library table */
	real_file_vprintf = OMRPORTLIB->file_vprintf;
	real_tty_startup = OMRPORTLIB->tty_startup;

	/* Override the file_vprintf function only, it will replace tty_startup since tty_vprintf
	 * won't return a value for verification
	 */
	OMRPORTLIB->file_vprintf = fake_omrfile_vprintf;

	/* Verify tty_startup was correctly overridden, the parameters passed should not matter */
	omrtty_vprintf_wrapper(OMRPORTLIB, format, 1, 2, 3);
	rc = OMRPORTLIB->tty_startup(OMRPORTLIB);

	/* Restore the function pointers */
	OMRPORTLIB->file_vprintf = real_file_vprintf;
	OMRPORTLIB->tty_startup = real_tty_startup ;

	if (J9TTY_STARTUP_PRIVATE_RETURN_VALUE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtty_vprintf() does not call omrfile_vprintf()\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library TTY operations.
 * Verify @ref omrtty.c::omrtty_get_chars.
 */
TEST(PortTTyTest, tty_test3)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtty_test3";

	reportTestEntry(OMRPORTLIB, testName);

	/* TODO implement */

	reportTestExit(OMRPORTLIB, testName);
}


/**
 * Verify port library TTY operations.
 * Verify @ref omrtty.c::omrtty_err_printf.
 *
 * @ref omrtty.c::omrtty_err_printf "omrtty_err_printf()" is a helper function for
 * @ref omrtty.c::omrtty_err_vprintf "omrtty_err_vprintf()".  Ensure that relationship is properly set up.
 */
TEST(PortTTyTest, tty_test4)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtty_test4";

	J9TTY_STARTUP_FUNC real_tty_startup;
	J9TTY_VPRINTF_FUNC real_tty_vprintf;
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	/* Save the real function pointers from the port library table */
	real_tty_vprintf = OMRPORTLIB->tty_vprintf;
	real_tty_startup = OMRPORTLIB->tty_startup;

	/* Override the file_vprintf function only, it will replace tty_startup since tty_vprintf
	 * won't return a value for verification
	 */
	OMRPORTLIB->tty_err_vprintf = fake_omrtty_vprintf;

	/* Verify tty_startup was correctly overridden, the parameters passed should not matter */
	omrtty_err_printf("You should not see this\n");
	rc = OMRPORTLIB->tty_startup(OMRPORTLIB);

	/* Restore the function pointers */
	OMRPORTLIB->tty_vprintf = real_tty_vprintf;
	OMRPORTLIB->tty_startup = real_tty_startup ;

	if (J9TTY_STARTUP_PRIVATE_RETURN_VALUE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtty_err_printf() does not call omrtty_err_vprintf()\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library TTY operations.
 *
 * @ref omrtty.c::omrtty_err_vprintf "omrtty_err_vprintf()" is a helper function for
 * @ref omrfile.c::omrfile_vprintf "omrfile_vprintf()".  Ensure that relationship is properly set up.
 */
TEST(PortTTyTest, tty_test5)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtty_test5";

	J9TTY_STARTUP_FUNC real_tty_startup;
	J9FILE_VPRINTF_FUNC real_file_vprintf;
	int32_t rc;

	reportTestEntry(OMRPORTLIB, testName);

	/* Save the real function pointers from the port library table */
	real_file_vprintf = OMRPORTLIB->file_vprintf;
	real_tty_startup = OMRPORTLIB->tty_startup;

	/* Override the file_vprintf function only, it will replace tty_startup since tty_vprintf
	 * won't return a value for verification
	 */
	OMRPORTLIB->file_vprintf = fake_omrfile_vprintf;

	/* Verify tty_startup was correctly overridden, the parameters passed should not matter */
	omrtty_err_printf("You should not see this\n");
	rc = OMRPORTLIB->tty_startup(OMRPORTLIB);

	/* Restore the function pointers */
	OMRPORTLIB->file_vprintf = real_file_vprintf;
	OMRPORTLIB->tty_startup = real_tty_startup ;

	if (J9TTY_STARTUP_PRIVATE_RETURN_VALUE != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtty_err_printf() does not call omrfile_vprintf()\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}


/**
 * Verify port library TTY operations.
 * Verify @ref omrtty.c::omrtty_available.
 */
TEST(PortTTyTest, tty_test6)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtty_test6";

	reportTestEntry(OMRPORTLIB, testName);

	/* TODO implement */

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library TTY operations.
 *
 * Print a message to the terminal.  If the user doesn't see anything then it has failed.
 * This test is included so the user has a better feel that things actually worked.  The
 * rest of the tests were verifying function relationships.
 */
TEST(PortTTyTest, tty_test7)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtty_test7";
	const char *format = "TTY printf, check check %d %d %d check ...\n";

	reportTestEntry(OMRPORTLIB, testName);

	/* Use printf to indicate something is expected on the terminal */
	omrtty_printf(format, 1, 2, 3);
	omrtty_printf("New line\n");

	reportTestExit(OMRPORTLIB, testName);
}
