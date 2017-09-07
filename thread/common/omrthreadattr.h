/*******************************************************************************
 * Copyright (c) 2007, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/


#ifndef J9THREADATTR_H_
#define J9THREADATTR_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform-specific definitions of omrthread_attr must include this
 * definition as a header.
 */
typedef struct omrthread_attr {
	uint32_t size; /* size of this structure */
	uint32_t category; /* thread category */
	uintptr_t stacksize;
	omrthread_schedpolicy_t schedpolicy;
	omrthread_prio_t priority; /* ignored if schedpolicy == INHERIT. */
	omrthread_detachstate_t detachstate;
	const char *name;
} omrthread_attr;

#ifdef __cplusplus
}
#endif

#endif /* J9THREADATTR_H_ */
