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

/**
 * @file
 * @ingroup Thread
 * @brief Thread creation attributes. May have platform-specific implementation.
 */
#include <stdlib.h>
#include "omrcfg.h" /* needed for platform-specific defines */
#include "omrcomp.h" /* needed for platform-specific defines */
#include "omrthread.h"
#include "threaddef.h"
#include "omrthreadattr.h"

#define J9THREAD_ATTR_IS_VALID(attr) ((attr) && (*(attr)) && ((*(attr))->size == sizeof(omrthread_attr)))
#define J9THREAD_VALUE_OUT_OF_RANGE(val, lo, hi) (((val) < (lo)) || ((val) > (hi)))

static intptr_t failedToSetAttr(intptr_t rc);

/**
 * Allocate an attribute. attr is not changed unless the function is successful.
 *
 * @param[out] attr
 * @return success or failure
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_INVALID_ATTR NULL attr passed in
 * @retval J9THREAD_ERR_NOMEMORY failed to allocate attr
 * @retval J9THREAD_ERR_INVALID_VALUE pthread_attr_t config failed
 */
intptr_t
omrthread_attr_init(omrthread_attr_t *attr)
{
	omrthread_attr_t newAttr;
	omrthread_library_t lib;

	if (!attr) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	lib = GLOBAL_DATA(default_library);

	newAttr = omrthread_allocate_memory(lib, sizeof(omrthread_attr), OMRMEM_CATEGORY_MM);
	if (!newAttr) {
		return J9THREAD_ERR_NOMEMORY;
	}
	newAttr->size = sizeof(omrthread_attr);

	if (failedToSetAttr(omrthread_attr_set_name(&newAttr, NULL))) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_schedpolicy(&newAttr, J9THREAD_SCHEDPOLICY_INHERIT))) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_priority(&newAttr, J9THREAD_PRIORITY_NORMAL))) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_stacksize(&newAttr, STACK_DEFAULT_SIZE))) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_detachstate(&newAttr, J9THREAD_CREATE_DETACHED))) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_category(&newAttr, J9THREAD_CATEGORY_SYSTEM_THREAD))) {
		goto destroy_attr;
	}

	*attr = newAttr;
	ASSERT(J9THREAD_ATTR_IS_VALID(attr));

	return J9THREAD_SUCCESS;

destroy_attr:
	omrthread_attr_destroy(&newAttr);
	return J9THREAD_ERR_INVALID_VALUE;
}

/**
 * Free an attribute. attr is not changed unless the function is successful.
 *
 * @param[in] attr
 * @return success or failure
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_INVALID_ATTR attr is an invalid attribute
 */
intptr_t
omrthread_attr_destroy(omrthread_attr_t *attr)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);

	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	omrthread_free_memory(lib, *attr);
	*attr = NULL;
	return J9THREAD_SUCCESS;
}

/**
 * Set name.
 *
 * If either the attribute or the attribute value are unsupported, the
 * name is still set in the attr structure.
 *
 * If an invalid attribute value is passed in, attr is unchanged.
 *
 * @param[in] attr
 * @param[in] name \0-terminated UTF8 string
 * @return success or failure
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_INVALID_ATTR attr is an invalid attribute
 * @retval J9THREAD_ERR_UNSUPPORTED_ATTR unsupported attribute
 */
intptr_t
omrthread_attr_set_name(omrthread_attr_t *attr, const char *name)
{
	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	(*attr)->name = name;

	/* no OS supports this attribute yet */
	return J9THREAD_ERR_UNSUPPORTED_ATTR;
}

/**
 * Set scheduling policy.
 *
 * If either the attribute or the attribute value are unsupported, the
 * name is still set in the attr structure.
 *
 * If an invalid attribute value is passed in, attr is unchanged.
 *
 * @param[in] attr
 * @param[in] policy
 * @return success or failure
 * @retval J9THREAD_ERR_INVALID_ATTR attr is an invalid attribute
 * @retval J9THREAD_ERR_INVALID_VALUE invalid policy value
 * @retval J9THREAD_ERR_UNSUPPORTED_VALUE unsupported policy value
 */
intptr_t
omrthread_attr_set_schedpolicy(omrthread_attr_t *attr, omrthread_schedpolicy_t policy)
{
	intptr_t rc = J9THREAD_SUCCESS;

	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	if ((policy < 0) || (policy >= omrthread_schedpolicy_LastEnum)) {
		return J9THREAD_ERR_INVALID_VALUE;
	}

	switch (policy) {
	case J9THREAD_SCHEDPOLICY_INHERIT:
	case J9THREAD_SCHEDPOLICY_OTHER:
		/* only INHERIT and OTHER are supported */
		(*attr)->schedpolicy = policy;
		rc = J9THREAD_SUCCESS;
		break;
	case J9THREAD_SCHEDPOLICY_FIFO:
	case J9THREAD_SCHEDPOLICY_RR:
		/* set the value, even if unsupported */
		(*attr)->schedpolicy = policy;
		rc = J9THREAD_ERR_UNSUPPORTED_VALUE;
		break;
	default:
		rc = J9THREAD_ERR_INVALID_VALUE;
		break;
	}

	return rc;
}

/**
 * Set priority.
 *
 * If either the attribute or the attribute value are unsupported, the
 * name is still set in the attr structure.
 *
 * If an invalid attribute value is passed in, attr is unchanged.
 *
 * @param[in] attr
 * @param[in] priority
 * @return success or failure
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_INVALID_ATTR attr is an invalid attribute
 * @retval J9THREAD_ERR_INVALID_VALUE invalid priority value
 */
intptr_t
omrthread_attr_set_priority(omrthread_attr_t *attr, omrthread_prio_t priority)
{
	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	if (J9THREAD_VALUE_OUT_OF_RANGE(priority, J9THREAD_PRIORITY_MIN, J9THREAD_PRIORITY_MAX)) {
		return J9THREAD_ERR_INVALID_VALUE;
	}

	(*attr)->priority = priority;
	return J9THREAD_SUCCESS;
}

/**
 * Set stack size.
 *
 * @param[in] attr
 * @param[in] stacksize A stacksize of 0 indicates the default size.
 * @return success or failure
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_INVALID_ATTR attr is an invalid attribute
 */
intptr_t
omrthread_attr_set_stacksize(omrthread_attr_t *attr, uintptr_t stacksize)
{
	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	if (0 == stacksize) {
		stacksize = STACK_DEFAULT_SIZE;
	}

	(*attr)->stacksize = stacksize;
	return J9THREAD_SUCCESS;
}

/* Doc is in thread_api.h */
intptr_t
omrthread_attr_set_detachstate(omrthread_attr_t *attr, omrthread_detachstate_t detachstate)
{
	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}
	(*attr)->detachstate = detachstate;
	return J9THREAD_SUCCESS;
}

/*
 * See thread_api.h for description
 */
intptr_t
omrthread_attr_set_category(omrthread_attr_t *attr, uint32_t category)
{
	intptr_t rc = J9THREAD_SUCCESS;

	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	switch (category) {
	case J9THREAD_CATEGORY_RESOURCE_MONITOR_THREAD:
	case J9THREAD_CATEGORY_SYSTEM_THREAD:
	case J9THREAD_CATEGORY_SYSTEM_GC_THREAD:
	case J9THREAD_CATEGORY_SYSTEM_JIT_THREAD:
	case J9THREAD_CATEGORY_APPLICATION_THREAD:
	case J9THREAD_USER_DEFINED_THREAD_CATEGORY_1:
	case J9THREAD_USER_DEFINED_THREAD_CATEGORY_2:
	case J9THREAD_USER_DEFINED_THREAD_CATEGORY_3:
	case J9THREAD_USER_DEFINED_THREAD_CATEGORY_4:
	case J9THREAD_USER_DEFINED_THREAD_CATEGORY_5:
		(*attr)->category = category;
		break;

	default:
		rc = J9THREAD_ERR_INVALID_VALUE;
		break;
	}

	return rc;
}

static intptr_t
failedToSetAttr(intptr_t rc)
{
	rc &= ~J9THREAD_ERR_OS_ERRNO_SET;
	return ((rc != J9THREAD_SUCCESS) && (rc != J9THREAD_ERR_UNSUPPORTED_ATTR));
}
