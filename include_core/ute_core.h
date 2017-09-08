/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

#ifndef _IBM_UTE_CORE_H
#define _IBM_UTE_CORE_H

/*
 * @ddr_namespace: map_to_type=UteCoreConstants
 */

#include <limits.h>

#include "omrcomp.h"
#include "omr.h"
#include "ute_module.h"

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * =============================================================================
 *  Constants
 * =============================================================================
 */
#define UT_VERSION                    5
#define UT_MODIFICATION               0
#define UT_TRACEFMT_VERSION_MOD       (float)1.2

/* Trace suspend/resume and disable/enable options. Suspend/resume is the external user facility,
 * disable/enable is the internal mechanism that trace, dump and memcheck components use. Both
 * mechanisms provide global (all threads) and current thread options.
 */
#define UT_SUSPEND_GLOBAL             1
#define UT_SUSPEND_THREAD             2
#define UT_RESUME_GLOBAL              1
#define UT_RESUME_THREAD              2

#define UT_DISABLE_GLOBAL             1
#define UT_DISABLE_THREAD             2
#define UT_ENABLE_GLOBAL              1
#define UT_ENABLE_THREAD              2

#define UT_APPLICATION_TRACE_MODULE 99


/*
 * =============================================================================
 *   Forward declarations for opaque types
 * =============================================================================
 */
typedef void (*UtListenerWrapper)(void *userData, OMR_VMThread *env, void **thrLocal,
	const char *modName, uint32_t traceId, const char *format, va_list varargs);

struct UtSubscription;

typedef omr_error_t (*utsSubscriberCallback)(struct UtSubscription *subscription);
typedef void (*utsSubscriberAlarmCallback)(struct UtSubscription *subscription);

/*
 * ======================================================================
 *  Trace buffer subscriber data
 * ======================================================================
 */
typedef struct UtSubscription {
	char                   *description;
	void                   *data;
	int32_t                  dataLength;
	volatile utsSubscriberCallback subscriber;
	volatile utsSubscriberAlarmCallback alarm;
	void                   *userData;
	/* internal fields - callers shouldn't modify these. */
	struct UtThreadData          **thr;
	int32_t                  threadPriority;
	int32_t                 threadAttach;
	struct UtSubscription         *next;
	struct UtSubscription         *prev;
	struct subscription     *queueSubscription;
	volatile uint32_t state;
} UtSubscription;

/*
 * =============================================================================
 *  UT data header
 * =============================================================================
 */
typedef struct UtDataHeader {
	char eyecatcher[4];
	int32_t length;
	int32_t version;
	int32_t modification;
} UtDataHeader;

/*
 * =============================================================================
 * Trace Buffer disk record
 * =============================================================================
 */
typedef struct UtTraceRecord {
	uint64_t sequence;		/* Time of latest entry           */
	uint64_t wrapSequence;	/* Time of last wrap              */
	uint64_t writePlatform;	/* Time of buffer write           */
	uint64_t writeSystem;	/* milliseconds since 1/1/70      */
	uint64_t threadId;		/* Thread identifier              */
	uint64_t threadSyn1;	/* Thread synonym 1               */
	uint64_t threadSyn2;	/* Thread synonym 2               */
	int32_t firstEntry;		/* Offset to first trace entry    */
	int32_t nextEntry;		/* Offset to next entry           */
	char threadName[1];		/* Thread name                    */
} UtTraceRecord;

/*
 * =============================================================================
 *  UT thread data
 * =============================================================================
 */
/* If you modify this structure, please update dbgext_utthreaddata in dbgTrc.c to cope with the
 * modifications.
 */
typedef struct UtThreadData {
	UtDataHeader	header;
	const void		*id;						/* Thread identifier               */
	const void		*synonym1;					/* Alternative thread identifier   */
	const void		*synonym2;					/* And another thread identifier   */
	const char		*name;						/* Thread name                     */
	unsigned char 	currentOutputMask;			/* Bitmask containing the options  */
	/* for the tracepoint currently being formatted*/
	struct UtTraceBuffer	*trcBuf;					/* Trace buffer                    */
	void			*external;					/* External trace thread local ptr */
	int32_t			suspendResume;				/* Suspend / resume count          */
	int				recursion;					/* Trace recursion indicator       */
	int				indent;						/* Iprint indentation count        */
} UtThreadData;

/*
 * =============================================================================
 *   The interface for calls out of UT to the client
 * =============================================================================
 */
#define UT_CLIENT_INTF_NAME "UTCI"
struct  UtClientInterface {
	UtDataHeader  header;
	/** DEAD STUB **/
};

/*
 * =============================================================================
 *   The server interface for calls into UT
 * =============================================================================
 */
#define UT_SERVER_INTF_NAME "UTSI"
/*
 * UtServerInterface is embedded in J9UtServerInterface.
 */
struct UtServerInterface {
	void (*TraceVNoThread)(UtModuleInfo *modInfo, uint32_t traceId, const char *spec, va_list varArgs);
	omr_error_t (*TraceSnap)(UtThreadData **thr, char *label, char **response);
	omr_error_t (*TraceSnapWithPriority)(UtThreadData **thr, char *label, int32_t snapPriority, char **response, int32_t sync);
	omr_error_t (*AddComponent)(UtModuleInfo *modInfo, const char **fmt);
	omr_error_t (*GetComponents)(UtThreadData **thr, char ***list, int32_t *number);
	omr_error_t (*GetComponent)(char *name, unsigned char **bitmap, int32_t *first, int32_t *last);
	int32_t (*Suspend)(UtThreadData **thr, int32_t type);
	int32_t (*Resume)(UtThreadData **thr, int32_t type);

	struct UtTracePointIterator *(*GetTracePointIteratorForBuffer)(UtThreadData **thr, const char *bufferName);
	char *(*FormatNextTracePoint)(struct UtTracePointIterator *iter, char *buffer, uint32_t buffLen);
	omr_error_t (*FreeTracePointIterator)(UtThreadData **thr, struct UtTracePointIterator *iter);
	omr_error_t (*RegisterRecordSubscriber)(UtThreadData **thr, const char *description, utsSubscriberCallback func, utsSubscriberAlarmCallback alarm, void *userData, struct UtTraceBuffer *start, struct UtTraceBuffer *stop, UtSubscription **record, int32_t attach);
	omr_error_t (*DeregisterRecordSubscriber)(UtThreadData **thr, UtSubscription *subscriptionID);
	omr_error_t (*FlushTraceData)(UtThreadData **thr, struct UtTraceBuffer **first, struct UtTraceBuffer **last, int32_t pause);
	omr_error_t (*GetTraceMetadata)(void **data, int32_t *length);
	void (*DisableTrace)(int32_t type);
	void (*EnableTrace)(int32_t type);
	omr_error_t (*RegisterTracePointSubscriber)(UtThreadData **thr, char *description, utsSubscriberCallback callback, utsSubscriberAlarmCallback alarm, void *userData, UtSubscription **subscription);
	omr_error_t (*DeregisterTracePointSubscriber)(UtThreadData **thr, UtSubscription *subscription);
	omr_error_t (*TraceRegister)(UtThreadData **thr, UtListenerWrapper func, void *userData);
	omr_error_t (*TraceDeregister)(UtThreadData **thr, UtListenerWrapper func, void *userData);
	omr_error_t (*SetOptions)(UtThreadData **thr, const char *opts[]);
};

#ifdef  __cplusplus
}
#endif
#endif /* !_IBM_UTE_CORE_H */
