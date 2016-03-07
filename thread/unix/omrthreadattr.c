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
 * @brief Thread creation attributes. Implementation for pthreads platforms.
 */
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>	/* for PTHREAD_STACK_MIN */
#if defined(LINUX) || defined(OSX)
#include <unistd.h> /* required for the _SC_PAGESIZE  constant */
#endif /* defined(LINUX) || defined(OSX) */
#include "omrcfg.h"
#include "omrcomp.h"
#include "omrthread.h"
#include "threaddef.h"
#include "unixpriority.h"
#include "unix/unixthreadattr.h"

#define J9THREAD_ATTR_IS_VALID(attr) ((attr) && (*(attr)) && ((*(attr))->size == sizeof(unixthread_attr)))
#define J9THREAD_VALUE_OUT_OF_RANGE(val, lo, hi) (((val) < (lo)) || ((val) > (hi)))

static intptr_t setStacksize(pthread_attr_t *pattr, uintptr_t stacksize);
static intptr_t setPriority(pthread_attr_t *pattr, omrthread_prio_t priority);
static intptr_t setSchedpolicy(pthread_attr_t *pattr, omrthread_schedpolicy_t policy);
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
	intptr_t rc = 0;
	unixthread_attr_t newAttr;
	omrthread_library_t lib;

	if (!attr) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	lib = GLOBAL_DATA(default_library);

	newAttr = omrthread_allocate_memory(lib, sizeof(unixthread_attr), OMRMEM_CATEGORY_MM);
	if (!newAttr) {
		return J9THREAD_ERR_NOMEMORY;
	}
	newAttr->hdr.size = sizeof(unixthread_attr);

	rc = DEBUG_SYSCALL(pthread_attr_init(&newAttr->pattr));
	if (rc != 0) {
		omrthread_free_memory(lib, newAttr);
		return J9THREAD_ERR_NOMEMORY;
	}

#if defined (RS6000)
	/* this is for AIXPPC */
	/* Solaris, by default, seems to use a fixed priority scheduling mechanism for its multiplexed threads which causes
	 * a running thread to never give up the CPU once it has started executing. By setting the PTHREAD_SCOPE_SYSTEM flag,
	 * we are telling the system to use the kernel's time-slicing scheduling algorithm to choose how this thread will be
	 * scheduled. Effectively the kernel, not the threads library will decide when this thread runs by scheduling it
	 * among all the other threads that are running on the system.
	 */
	rc = DEBUG_SYSCALL(pthread_attr_setscope(&newAttr->pattr, PTHREAD_SCOPE_SYSTEM));
	if (rc != 0) {
		goto destroy_attr;
	}
	/* Note: AIX 6.1 will default to system scope */
#endif

	if (failedToSetAttr(omrthread_attr_set_name((omrthread_attr_t *)&newAttr, NULL))) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_schedpolicy((omrthread_attr_t *)&newAttr, J9THREAD_SCHEDPOLICY_INHERIT))) {
		goto destroy_attr;
	}

	/* set priority after policy in case RTJ wants to override the policy */
	if (failedToSetAttr(omrthread_attr_set_priority((omrthread_attr_t *)&newAttr, J9THREAD_PRIORITY_NORMAL))) {
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_stacksize((omrthread_attr_t *)&newAttr, STACK_DEFAULT_SIZE))) {
		goto destroy_attr;
	}

	/*
	 * VMDesign 1647. Disable thread priorities and policies.
	 */
	if (lib->flags & J9THREAD_LIB_FLAG_NO_SCHEDULING) {
		rc = DEBUG_SYSCALL(pthread_attr_setinheritsched(&newAttr->pattr, PTHREAD_INHERIT_SCHED));
		if (rc != 0) {
			goto destroy_attr;
		}
	}

	if (failedToSetAttr(omrthread_attr_set_detachstate((omrthread_attr_t *)&newAttr, J9THREAD_CREATE_DETACHED))) {
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
 * @retval J9THREAD_ERR_INVALID_ATTR attr is an invalid attribute
 * @retval J9THREAD_ERR_INVALID_VALUE invalid policy value
 * @retval J9THREAD_ERR_UNSUPPORTED_VALUE unsupported policy value
 *
 * @note HACK: For RTJ, the value set here is ignored.
 *
 * @see omrthread_attr_set_priority
 */
intptr_t
omrthread_attr_set_schedpolicy(omrthread_attr_t *attr, omrthread_schedpolicy_t policy)
{
	intptr_t rc;
	unixthread_attr_t ux;
	omrthread_library_t lib;

	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	/*
	 * VMDesign 1647. Disable thread priorities and policies.
	 */
	lib = GLOBAL_DATA(default_library);
	if (lib->flags & J9THREAD_LIB_FLAG_NO_SCHEDULING) {
		return J9THREAD_SUCCESS;
	}

	ux = *(unixthread_attr_t *)attr;

	if (omrthread_lib_use_realtime_scheduling()) {
		if (policy >= omrthread_schedpolicy_LastEnum) {
			return J9THREAD_ERR_INVALID_VALUE;
		}
		/* don't modify the pthread_attr_t */
	} else {
		rc = setSchedpolicy(&ux->pattr, policy);
		if (rc != J9THREAD_SUCCESS) {
			return rc;
		}
	}

	ux->hdr.schedpolicy = policy;
	return J9THREAD_SUCCESS;
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
 *
 * @note HACK: For RTJ, omrthread_attr_set_priority() sets the scheduling policy.
 * It doesn't set omrthread_attr_t.schedpolicy, but the schedpolicy value is ignored.
 *
 * @see omrthread_attr_set_schedpolicy
 */
intptr_t
omrthread_attr_set_priority(omrthread_attr_t *attr, omrthread_prio_t priority)
{
	intptr_t rc;
	unixthread_attr_t ux;
	omrthread_library_t lib;

	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	/*
	 * VMDesign 1647. Disable thread priorities and policies.
	 */
	lib = GLOBAL_DATA(default_library);
	if (lib->flags & J9THREAD_LIB_FLAG_NO_SCHEDULING) {
		return J9THREAD_SUCCESS;
	}

	if (J9THREAD_VALUE_OUT_OF_RANGE(priority, J9THREAD_PRIORITY_MIN, J9THREAD_PRIORITY_MAX)) {
		return J9THREAD_ERR_INVALID_VALUE;
	}

	ux = *(unixthread_attr_t *)attr;

	rc = setPriority(&ux->pattr, priority);
	if (rc != J9THREAD_SUCCESS) {
		return rc;
	}

	ux->hdr.priority = priority;
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
 * @retval J9THREAD_ERR_INVALID_VALUE invalid stack size
 */
intptr_t
omrthread_attr_set_stacksize(omrthread_attr_t *attr, uintptr_t stacksize)
{
	intptr_t rc;
	unixthread_attr_t ux;

	if (!J9THREAD_ATTR_IS_VALID(attr)) {
		return J9THREAD_ERR_INVALID_ATTR;
	}

	ux = *(unixthread_attr_t *)attr;

	if (0 == stacksize) {
		stacksize = STACK_DEFAULT_SIZE;
	}

	rc = setStacksize(&ux->pattr, stacksize);
	if (rc != J9THREAD_SUCCESS) {
		return rc;
	}

	ux->hdr.stacksize = stacksize;
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

static intptr_t
setSchedpolicy(pthread_attr_t *pattr, omrthread_schedpolicy_t policy)
{
	intptr_t rc = J9THREAD_SUCCESS;
	intptr_t pret = 0;

	if (J9THREAD_SCHEDPOLICY_INHERIT == policy) {
#if defined(AIXPPC) || defined(LINUX) || defined(OSX)
		/*
		 * Non-root users are frequently not allowed to create threads
		 * using SCHED_RR or SCHED_FIFO.
		 *
		 * The docs say that the schedpolicy should be ignored if PTHREAD_INHERIT_SCHED
		 * is enabled, but it appears that leaving one of the privileged schedpolicies
		 * set in the pthread_attr_t still causes thread creation to fail. This bug
		 * has been observed on AIX 5.3, Linux 2.4, and Linux 2.6.
		 *
		 * It's ok that the pthread_attr_t doesn't match the omrthread_attr_t
		 * because the user can only turn off PTHREAD_INHERIT_SCHED by
		 * asking for a different scheduling policy.
		 */
		pret = DEBUG_SYSCALL(pthread_attr_setschedpolicy(pattr, SCHED_OTHER));
		if (pret != 0) {
			return J9THREAD_ERR_INVALID_VALUE;
		}
#endif /* defined(AIXPPC) || defined(LINUX) || defined(OSX) */
		pret = DEBUG_SYSCALL(pthread_attr_setinheritsched(pattr, PTHREAD_INHERIT_SCHED));
		if (pret != 0) {
			return J9THREAD_ERR_INVALID_VALUE;
		}

	} else {
		int ospolicy;

		switch (policy) {
		case J9THREAD_SCHEDPOLICY_INHERIT:
			/* shouldn't happen */
			rc = J9THREAD_ERR_INVALID_VALUE;
			break;
		case J9THREAD_SCHEDPOLICY_OTHER:
			ospolicy = SCHED_OTHER;
			break;
		case J9THREAD_SCHEDPOLICY_RR:
			ospolicy = SCHED_RR;
			break;
		case J9THREAD_SCHEDPOLICY_FIFO:
			ospolicy = SCHED_FIFO;
			break;
		default:
			rc = J9THREAD_ERR_INVALID_VALUE;
			break;
		}

		if (J9THREAD_SUCCESS == rc) {
			pret = DEBUG_SYSCALL(pthread_attr_setinheritsched(pattr, PTHREAD_EXPLICIT_SCHED));
			if (pret != 0) {
				return J9THREAD_ERR_INVALID_VALUE;
			}
			pret = DEBUG_SYSCALL(pthread_attr_setschedpolicy(pattr, ospolicy));
			if (pret != 0) {
				return J9THREAD_ERR_INVALID_VALUE;
			}
		}
	}

	return rc;
}

static intptr_t
setPriority(pthread_attr_t *pattr, omrthread_prio_t priority)
{
	intptr_t rc;
	struct sched_param param;

	if (omrthread_lib_use_realtime_scheduling()) {
		/* The schedpolicy is encoded in the upper bits of the priority value. */

		rc = DEBUG_SYSCALL(pthread_attr_setinheritsched(pattr, PTHREAD_EXPLICIT_SCHED));
		if (rc != 0) {
			return J9THREAD_ERR_INVALID_VALUE;
		}

		rc = DEBUG_SYSCALL(pthread_attr_setschedpolicy(pattr, omrthread_get_scheduling_policy(priority)));
		if (rc != 0) {
			return J9THREAD_ERR_INVALID_VALUE;
		}
	}

	/* for non-realtime JVMs, the setting of the priority here may have no effect,
	 * since we may have previously called pthread_attr_setinheritsched with PTHREAD_INHERIT_SCHED in setSchedpolicy,
	 * however there is no harm in setting the priority here and checking the value is valid.
	 */
	rc = DEBUG_SYSCALL(pthread_attr_getschedparam(pattr, &param));
	if (rc != 0) {
		return J9THREAD_ERR_INVALID_ATTR;
	}
	param.sched_priority = omrthread_get_mapped_priority(priority);
	rc = DEBUG_SYSCALL(pthread_attr_setschedparam(pattr, &param));
	if (rc != 0) {
		return J9THREAD_ERR_INVALID_VALUE;
	}

	return J9THREAD_SUCCESS;
}

/*
 * stacksize must not be 0.
 */
static intptr_t
setStacksize(pthread_attr_t *pattr, uintptr_t stacksize)
{
	/* stacksize may be adjusted to satisfy platform-specific quirks */

#if defined(LINUX) || defined(OSX)
	/* Linux allocates 2MB if you ask for a stack smaller than STACK_MIN */
	{
		long pageSafeMinimumStack = 2 * sysconf(_SC_PAGESIZE);

		if (pageSafeMinimumStack < PTHREAD_STACK_MIN) {
			pageSafeMinimumStack = PTHREAD_STACK_MIN;
		}
		if (stacksize < pageSafeMinimumStack) {
			stacksize = pageSafeMinimumStack;
		}
	}
#endif /* defined(LINUX) || defined(OSX) */


	if (DEBUG_SYSCALL(pthread_attr_setstacksize(pattr, stacksize)) != 0) {
		return J9THREAD_ERR_INVALID_VALUE;
	}
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
