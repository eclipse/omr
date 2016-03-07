/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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
