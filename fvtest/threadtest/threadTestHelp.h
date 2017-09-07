/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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

#ifndef THREADTESTHELP_H_INCLUDED
#define THREADTESTHELP_H_INCLUDED

#include <stdio.h>
#include <stdint.h>

#include "omrTest.h"
#include "thread_api.h"
#include "testHelper.hpp"

extern ThreadTestEnvironment *omrTestEnv;

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#define VERBOSE_JOIN(_threadToJoin, _expectedRc) \
	do { \
		intptr_t _rc = 0; \
		EXPECT_EQ((_expectedRc), _rc = omrthread_join(_threadToJoin)); \
		if (_rc & J9THREAD_ERR_OS_ERRNO_SET) { \
			omrTestEnv->log(LEVEL_ERROR, "omrthread_join() returned os_errno=%d\n", (int)omrthread_get_os_errno()); \
		} \
	} while(0)

void createJoinableThread(omrthread_t *newThread, omrthread_entrypoint_t entryProc, void *entryArg);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* THREADTESTHELP_H_INCLUDED */
