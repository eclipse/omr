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


#ifndef THREADTESTHELP_H_INCLUDED
#define THREADTESTHELP_H_INCLUDED

#include <stdio.h>
#include <stdint.h>

#include "omrTest.h"
#include "thread_api.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#define VERBOSE_JOIN(_threadToJoin, _expectedRc) \
	do { \
		intptr_t _rc = 0; \
		EXPECT_EQ((_expectedRc), _rc = omrthread_join(_threadToJoin)); \
		if (_rc & J9THREAD_ERR_OS_ERRNO_SET) { \
			printf("omrthread_join() returned os_errno=%d\n", (int)omrthread_get_os_errno()); \
		} \
	} while(0)

void createJoinableThread(omrthread_t *newThread, omrthread_entrypoint_t entryProc, void *entryArg);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* THREADTESTHELP_H_INCLUDED */
