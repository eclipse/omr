/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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

typedef union sigval sigval_t;

typedef ucontext_t thread_context;

typedef struct AIXFunctionEpilogue {
	uint32_t nullWord;
	struct tbtable_short tracebackTable;
};

typedef struct AIXStackFrame {
	struct AIXStackFrame *link;
	void *toc;
	void *iar;
} AIXStackFrame;

#endif
