/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2007, 2016
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
