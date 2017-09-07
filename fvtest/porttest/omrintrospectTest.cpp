/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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
	portTestEnv->changeIndent(1);

	reportTestEntry(OMRPORTLIB, testName);
#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
	portTestEnv->log("test legal offsets\n");
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
	portTestEnv->log("test negative offset\n");
	status = omrintrospect_set_suspend_signal_offset(-1);
	if (0 == status) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrintrospect_set_suspend_signal_offset with negative offset succeeded unexpectedly");
	}

	portTestEnv->log("test excessive offset\n");
	status = omrintrospect_set_suspend_signal_offset(SIGRTMAX);
	if (0 == status) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrintrospect_set_suspend_signal_offset out of range offset succeeded unexpectedly");
	}
#else /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */
	portTestEnv->log("verify that omrintrospect_set_suspend_signal_offset returns failure on unsupported platforms\n");
	status = omrintrospect_set_suspend_signal_offset(0);
	portTestEnv->log("omrintrospect_set_suspend_signal_offset returns %d \n", status);
	if (0 == status) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrintrospect_set_suspend_signal_offset succeeded unexpectedly");
	}
#endif /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */
	portTestEnv->changeIndent(-1);
}
