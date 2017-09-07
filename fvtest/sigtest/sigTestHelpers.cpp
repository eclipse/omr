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

#include "sigTestHelpers.hpp"

bool
handlerIsFunction(sighandler_t handler)
{
	return (SIG_DFL != handler) && (SIG_IGN != handler) && (NULL != handler);
}

#if !defined(WIN32)
bool
handlerIsFunction(const struct sigaction *act)
{
	return handlerIsFunction(act->sa_handler)
		   || ((act->sa_flags & SA_SIGINFO)
			   && (SIG_DFL != (sighandler_t)act->sa_sigaction)
			   && (SIG_IGN != (sighandler_t)act->sa_sigaction));
}
#endif /* !defined(WIN32) */

void
createThread(omrthread_t *newThread, uintptr_t suspend, omrthread_detachstate_t detachstate,
			 omrthread_entrypoint_t entryProc, void *entryArg)
{
	omrthread_attr_t attr = NULL;
	intptr_t rc = 0;

	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_init(&attr));
	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_set_detachstate(&attr, detachstate));
	EXPECT_EQ(J9THREAD_SUCCESS,
			  rc = omrthread_create_ex(newThread, &attr, suspend, entryProc, entryArg));
	if (rc & J9THREAD_ERR_OS_ERRNO_SET) {
		printf("omrthread_create_ex() returned os_errno=%d\n", (int)omrthread_get_os_errno());
	}
	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_destroy(&attr));
}

intptr_t
joinThread(omrthread_t threadToJoin)
{
	intptr_t rc = J9THREAD_SUCCESS;

	EXPECT_EQ(J9THREAD_SUCCESS, rc = omrthread_join(threadToJoin));
	if (rc & J9THREAD_ERR_OS_ERRNO_SET) {
		printf("omrthread_join() returned os_errno=%d\n", (int)omrthread_get_os_errno());
	}
	return rc;
}
