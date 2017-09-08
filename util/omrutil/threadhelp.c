/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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

#include "omr.h"
#include "omrutil.h"
#include "thread_api.h"

static intptr_t failedToSetAttr(intptr_t rc);

/*
 * See omrutil.h for doc
 */
intptr_t
createThreadWithCategory(omrthread_t *handle, uintptr_t stacksize, uintptr_t priority, uintptr_t suspend,
						 omrthread_entrypoint_t entrypoint, void *entryarg, uint32_t category)
{
	omrthread_attr_t attr;
	intptr_t rc = J9THREAD_SUCCESS;

	if (J9THREAD_SUCCESS != omrthread_attr_init(&attr)) {
		return J9THREAD_ERR_CANT_ALLOC_CREATE_ATTR;
	}

	if (failedToSetAttr(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_OTHER))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	/* HACK: the priority must be set after the policy because RTJ might override the schedpolicy */
	if (failedToSetAttr(omrthread_attr_set_priority(&attr, priority))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_stacksize(&attr, stacksize))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_category(&attr, category))) {
		rc = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	rc = omrthread_create_ex(handle, &attr, suspend, entrypoint, entryarg);

destroy_attr:
	omrthread_attr_destroy(&attr);
	return rc;
}

/*
 * See omrutil.h for doc
 */
intptr_t
attachThreadWithCategory(omrthread_t *handle, uint32_t category)
{
	omrthread_attr_t attr;
	intptr_t rc = J9THREAD_SUCCESS;

	if (J9THREAD_SUCCESS != omrthread_attr_init(&attr)) {
		return J9THREAD_ERR_CANT_ALLOC_ATTACH_ATTR;
	}

	if (failedToSetAttr(omrthread_attr_set_category(&attr, category))) {
		rc = J9THREAD_ERR_INVALID_ATTACH_ATTR;
		goto destroy_attr;
	}

	rc = omrthread_attach_ex(handle, &attr);

destroy_attr:
	omrthread_attr_destroy(&attr);
	return rc;
}

static intptr_t
failedToSetAttr(intptr_t rc)
{
	rc &= ~J9THREAD_ERR_OS_ERRNO_SET;
	return ((rc != J9THREAD_SUCCESS) && (rc != J9THREAD_ERR_UNSUPPORTED_ATTR));
}
