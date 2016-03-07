/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015
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
 ******************************************************************************/

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
