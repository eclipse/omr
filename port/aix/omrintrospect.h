/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief process introspection support
 */
#ifndef J9INTROSPECT_INCLUDE
#define J9INTROSPECT_INCLUDE

#include <sys/debug.h>
#include "omrintrospect_common.h"

/* AIX uses sigpthreadmask for per thread masks */
#define sigprocmask sigthreadmask

#ifndef _AIX61
/* These functions are system calls but are not defined in any of the system headers so we have to declare them here */
extern int getthrds(pid_t ProcessIdentifier, struct thrdsinfo64 *ThreadBuffer, int ThreadSize, tid_t *IndexPointer, int Count);
extern int getthrds64(pid_t ProcessIdentifier, struct thrdentry64 *ThreadBuffer, int ThreadSize, tid64_t *IndexPointer, int Count);
#endif


extern int32_t __omrgetsp(void);

typedef ucontext_t thread_context;

typedef struct AIXFunctionEpilogue {
	uint32_t nullWord;
	struct tbtable_short tracebackTable;
} AIXFunctionEpilogue;

typedef struct AIXStackFrame {
	struct AIXStackFrame *link;
	void *toc;
	void *iar;
} AIXStackFrame;

#endif
