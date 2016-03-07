/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016, 2016
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
 * @brief Verify port library introspection operations.
 *
 * Exercise the API for port library introspection operations.  These functions
 * can be found in the file @ref omrintrospect.c
 */

#include "omrport.h"
#if defined(LINUX)
#include <signal.h>
#endif /* defined(LINUX) */
#include "testHelpers.hpp"

/**
 * Verify setting of suspend signal
 * @Note this assumes we use SIGRTMIN...SIGRTMAX
 */
TEST(PortIntrospectTest, introspect_test_set_signal_offset)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "introspect_test_set_signal_offset";
	int32_t status = 0;

	reportTestEntry(OMRPORTLIB, testName);
#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
	outputComment(OMRPORTLIB, "test legal offsets\n");
	for (int32_t i = 0; i <= SIGRTMAX-SIGRTMIN; ++i) {
#if defined(SIG_RI_INTERRUPT_INDEX)
		if (SIG_RI_INTERRUPT_INDEX == signalOffset) {
			continue;
		}
#endif  /* defined(SIG_RI_INTERRUPT_INDEX) */
		status = omrintrospect_set_suspend_signal_offset(i);
		if (0 != status) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrintrospect_set_suspend_signal_offset failed unexpectedly");
		}
	}
	outputComment(OMRPORTLIB, "test negative offset\n");
	status = omrintrospect_set_suspend_signal_offset(-1);
	if (0 == status) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrintrospect_set_suspend_signal_offset with negative offset succeeded unexpectedly");
	}

	outputComment(OMRPORTLIB, "test excessive offset\n");
	status = omrintrospect_set_suspend_signal_offset(SIGRTMAX);
	if (0 == status) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrintrospect_set_suspend_signal_offset out of range offset succeeded unexpectedly");
	}
#else /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */
	outputComment(OMRPORTLIB, "verify that omrintrospect_set_suspend_signal_offset returns failure on unsupported platforms\n");
	status = omrintrospect_set_suspend_signal_offset(0);
	outputComment(OMRPORTLIB, "omrintrospect_set_suspend_signal_offset returns %d \n", status);
	if (0 == status) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrintrospect_set_suspend_signal_offset succeeded unexpectedly");
	}
#endif /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */
}


