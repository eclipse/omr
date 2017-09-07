/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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

#include "threadExtendedTestHelpers.hpp"

void
thrExtendedTestSetUp(OMRPortLibrary *portLibrary)
{
	intptr_t rc = omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT);
	ASSERT_EQ(0, rc) << "omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT) failed\n";

	/* initialize port Library */
	rc = (int)omrport_init_library(portLibrary, sizeof(OMRPortLibrary));
	ASSERT_EQ(0, rc) << "omrport_init_library(&_OMRPortLibrary, sizeof(OMRPortLibrary)) failed\n";
}

void
thrExtendedTestTearDown(OMRPortLibrary *portLibrary)
{
	int rc = portLibrary->port_shutdown_library(portLibrary);
	ASSERT_EQ(0, rc) << "port_shutdown_library() failed\n";

	omrthread_detach(NULL);
}
