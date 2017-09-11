/*******************************************************************************
 * Copyright (c) 2007, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


#ifndef UNIXTHREADATTR_H_
#define UNIXTHREADATTR_H_

#include <pthread.h>
#include "common/omrthreadattr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* use the same levels of indirection as omrthread_attr_t definition,
 * for consistency
 */
typedef struct unixthread_attr {
	omrthread_attr hdr; /* must be first member */
	pthread_attr_t pattr;
} unixthread_attr;

typedef struct unixthread_attr *unixthread_attr_t;

#ifdef __cplusplus
}
#endif

#endif /* UNIXTHREADATTR_H_ */
