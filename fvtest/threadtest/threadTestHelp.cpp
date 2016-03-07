/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#include "threadTestHelp.h"

#include <stdint.h>

#include "omrTest.h"
#include "thread_api.h"

void
createJoinableThread(omrthread_t *newThread, omrthread_entrypoint_t entryProc, void *entryArg)
{
	omrthread_attr_t attr = NULL;
	intptr_t rc = 0;

	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_init(&attr));
	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_set_detachstate(&attr, J9THREAD_CREATE_JOINABLE));
	EXPECT_EQ(J9THREAD_SUCCESS,
			  rc = omrthread_create_ex(newThread, &attr, 0, entryProc, entryArg));
	if (rc & J9THREAD_ERR_OS_ERRNO_SET) {
		printf("omrthread_create_ex() returned os_errno=%d\n", (int)omrthread_get_os_errno());
	}
	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_destroy(&attr));
}
