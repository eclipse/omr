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
