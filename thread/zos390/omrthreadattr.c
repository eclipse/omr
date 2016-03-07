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

/**
 * @file
 * @ingroup Thread
 * @brief Thread creation attributes. Implementation for z/OS.
 */
#include <pthread.h>
#include <stdlib.h>
#include "omrthread.h"
#include "threaddef.h"
#include "unix/unixthreadattr.h"

#define J9THREAD_ATTR_IS_VALID(attr) ((attr) && (*(attr)) && ((*(attr))->size == sizeof(unixthread_attr)))
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
	intptr_t rc;
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	unixthread_attr_t newAttr;
	int threadWeight = __MEDIUM_WEIGHT; /* default to medium */

	if (!attr) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	/* get the thread weight */
	if (NULL == lib->thread_weight) {
		lib->thread_weight = (char *)*omrthread_global("thread_weight");

		/*
		 * CMVC 106682. Do not set the library default if
		 * threadParseArguments() has not yet run.
		 */
	}

	if (NULL != lib->thread_weight) {
		/*
		 * We only support medium and heavy.
		 * threadParseArguments() ensures we have
		 * "medium" or "heavy"
		 */
		if ('h' == lib->thread_weight[0]) {
			threadWeight = __HEAVY_WEIGHT;
		}
	}

	newAttr = omrthread_allocate_memory(lib, sizeof(unixthread_attr), OMRMEM_CATEGORY_THREADS);
	if (!newAttr) {
		return J9THREAD_ERR_NOMEMORY;
	}
	newAttr->hdr.size = sizeof(unixthread_attr);

	rc = DEBUG_SYSCALL(pthread_attr_init(&newAttr->pattr));
	if (rc != 0) {
		omrthread_free_memory(lib, newAttr);
		return J9THREAD_ERR_NOMEMORY;
	}

	if (failedToSetAttr(omrthread_attr_set_detachstate((omrthread_attr_t *)&newAttr, J9THREAD_CREATE_DETACHED))) {
		goto destroy_attr;
	}

	rc = DEBUG_SYSCALL(pthread_attr_setweight_np(&newAttr->pattr, threadWeight));
	if (rc != 0) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_name((omrthread_attr_t *)&newAttr, NULL))) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_schedpolicy((omrthread_attr_t *)&newAttr, J9THREAD_SCHEDPOLICY_INHERIT))) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_priority((omrthread_attr_t *)&newAttr, J9THREAD_PRIORITY_NORMAL))) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_stacksize((omrthread_attr_t *)&newAttr, STACK_DEFAULT_SIZE))) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_category((omrthread_attr_t *)&newAttr, J9THREAD_CATEGORY_SYSTEM_THREAD))) {
		goto destroy_attr;
	}

	*attr = (omrthread_attr_t)newAttr;
	ASSERT(J9THREAD_ATTR_IS_VALID(attr));

	return J9THREAD_SUCCESS;

destroy_attr:
	omrthread_attr_destroy((omrthread_attr_t *)&newAttr);
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
	unixthread_attr_t ux;
	omrthread_library_t lib = GLOBAL_DATA(default_library);

	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}
	ux = *(unixthread_attr_t *)attr;

	DEBUG_SYSCALL(pthread_attr_destroy(&ux->pattr));
	omrthread_free_memory(lib, ux);
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
 */
intptr_t
omrthread_attr_set_schedpolicy(omrthread_attr_t *attr, omrthread_schedpolicy_t policy)
{
	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	if ((policy < 0) || (policy >= omrthread_schedpolicy_LastEnum)) {
		return J9THREAD_ERR_INVALID_VALUE;
	}

	(*attr)->schedpolicy = policy;
	return J9THREAD_ERR_UNSUPPORTED_ATTR;
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
	return J9THREAD_ERR_UNSUPPORTED_ATTR;
}

/**
 * Set stack size.
 *
 * @param[in] attr
 * @param[in] stacksize A stacksize of 0 indicates the default stack size.
 * @return success or failure
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_INVALID_ATTR attr is an invalid attribute
 * @retval J9THREAD_ERR_INVALID_VALUE invalid stack size
 */
intptr_t
omrthread_attr_set_stacksize(omrthread_attr_t *attr, uintptr_t stacksize)
{
	unixthread_attr_t ux;

	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}
	ux = *(unixthread_attr_t *)attr;

	if (0 == stacksize) {
		stacksize = STACK_DEFAULT_SIZE;
	}

	if (DEBUG_SYSCALL(pthread_attr_setstacksize(&ux->pattr, stacksize)) != 0) {
		return J9THREAD_ERR_INVALID_VALUE;
	}

	ux->hdr.stacksize = stacksize;
	return J9THREAD_SUCCESS;
}

/* Doc is in thread_api.h */
intptr_t
omrthread_attr_set_detachstate(omrthread_attr_t *attr, omrthread_detachstate_t detachstate)
{
	intptr_t rc = J9THREAD_SUCCESS;
	unixthread_attr_t ux = NULL;
	int pthreadDetachstate = ((J9THREAD_CREATE_DETACHED == detachstate)? 1: 0);

	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}
	ux = *(unixthread_attr_t *)attr;

	if (0 != DEBUG_SYSCALL(pthread_attr_setdetachstate(&ux->pattr, &pthreadDetachstate))) {
		rc = J9THREAD_ERR_INVALID_VALUE;
	} else {
		(*attr)->detachstate = detachstate;
	}
	return rc;
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
