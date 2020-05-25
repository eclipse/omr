/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef OMRHOOKABLE_H
#define OMRHOOKABLE_H

/*
 * @ddr_namespace: default
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrthread.h"

#define J9_EVENT_IS_HOOKED(interface, event) (((uint8_t*)(&(interface).common + 1))[(event) & J9HOOK_EVENT_NUM_MASK] & J9HOOK_FLAG_HOOKED)
#define J9_EVENT_IS_RESERVED(interface, event) (((uint8_t*)(&(interface).common + 1))[(event) & J9HOOK_EVENT_NUM_MASK] & J9HOOK_FLAG_RESERVED)

/* derives the common interface pointer from a module-specific hook interface */
#define J9_HOOK_INTERFACE(interface) (&(interface).common.hookInterface)

#define J9HOOK_LIB_CONTROL_TRACE_START "trace_start"
#define J9HOOK_LIB_CONTROL_TRACE_STOP "trace_stop"

intptr_t
omrhook_lib_control(const char *key, uintptr_t value);

struct J9HookInterface; /* Forward struct declaration */
typedef void (*J9HookFunction)(struct J9HookInterface **hookInterface, uintptr_t eventNum, void *eventData, void *userData); /* Forward struct declaration */
typedef struct J9HookInterface {
	void (*J9HookDispatch)(struct J9HookInterface **hookInterface, uintptr_t eventNum, void *eventData);
	intptr_t (*J9HookDisable)(struct J9HookInterface **hookInterface, uintptr_t eventNum);
	intptr_t (*J9HookReserve)(struct J9HookInterface **hookInterface, uintptr_t eventNum);
	intptr_t (*J9HookRegister)(struct J9HookInterface **hookInterface, uintptr_t eventNum, J9HookFunction function, void *userData, ...);
	intptr_t (*J9HookRegisterWithCallSite)(struct J9HookInterface **hookInterface, uintptr_t eventNum, J9HookFunction function, const char *callsite, void *userData, ...);
	void (*J9HookUnregister)(struct J9HookInterface **hookInterface, uintptr_t eventNum, J9HookFunction function, void *userData);
	void (*J9HookShutdownInterface)(struct J9HookInterface **hookInterface);
	intptr_t (*J9HookIsEnabled)(struct J9HookInterface **hookInterface, uintptr_t eventNum);
	uintptr_t (*J9HookAllocateAgentID)(struct J9HookInterface **hookInterface);
	void (*J9HookDeallocateAgentID)(struct J9HookInterface **hookInterface, uintptr_t agentID);
} J9HookInterface;


#define J9HOOK_ERR_DISABLED  -1
#define J9HOOK_ERR_NOMEM  -2
#define J9HOOK_ERR_INVALID_AGENT_ID  -3
#define J9HOOK_TAG_REVERSE_ORDER  0x10000000
#define J9HOOK_TAG_AGENT_ID  0x20000000
#define J9HOOK_TAG_COUNTED  0x40000000
#define J9HOOK_TAG_ONCE  0x80000000
#define J9HOOK_TAG_SAMPLING_MASK	0x00ff0000
#define J9HOOK_AGENTID_FIRST  ((uintptr_t)0)
#define J9HOOK_AGENTID_DEFAULT  ((uintptr_t)1)
#define J9HOOK_AGENTID_LAST  ((uintptr_t)-1)

/* time threshold (=100 milliseconds) for triggering the tracepoint  */
#define OMRHOOK_DEFAULT_THRESHOLD_IN_MICROSECONDS_WARNING_CALLBACK_ELAPSED_TIME	(100 * 1000)

/* array of OMREventInfo4Dump is added in individual hookInterface by Hook generation tool to avoid
   rumtime native memory allocation(malloc), use this macro to access &infos4Dump[event]
   locate infos4Dump from end of interface to avoid alignment calculating
*/

#define J9HOOK_DUMPINFO(interface, event) (&((OMREventInfo4Dump *)&(((J9HookRecord **)((uint8_t*)(interface) + (interface)->size ))[0 - (interface)->eventSize]))[event - (interface)->eventSize])

typedef struct OMRHookInfo4Dump {
	const char *callsite;
	void * func_ptr;		/* use function pointer instead, if callsite == NULL */
	uint64_t startTime;
	uint64_t duration;
}OMRHookInfo4Dump;

typedef struct OMREventInfo4Dump {
	struct OMRHookInfo4Dump longestHook;
	struct OMRHookInfo4Dump lastHook;
	volatile uintptr_t count;
	volatile uintptr_t totalTime;
}OMREventInfo4Dump;

typedef struct J9CommonHookInterface {
	struct J9HookInterface *hookInterface;
	uintptr_t size;
	omrthread_monitor_t lock;
	struct J9Pool *pool;
	uintptr_t nextAgentID;
	struct OMRPortLibrary *portLib;		/* for accessing PortLibrary  */
	uint64_t threshold4Trace;			/* the threshold for triggering tracepoint */
	uintptr_t eventSize;				/* how many events supported by this hook interface */
} J9CommonHookInterface;


#define J9HOOK_FLAG_DISABLED  4
#define J9HOOK_EVENT_NUM_MASK  0xFFFF
#define J9HOOK_FLAG_HOOKED  1
#define J9HOOK_FLAG_RESERVED  2

typedef struct J9HookRecord {
	struct J9HookRecord *next;
	J9HookFunction function;
	const char *callsite;
	void *userData;
	uintptr_t count;
	uintptr_t id;
	uintptr_t agentID;
} J9HookRecord;


/* magic hooks supported by every hook interface */

/*
 * J9HOOK_REGISTRATION_EVENT
 * Invoked every time a listener is succesfully registered or unregistered for an event (including this one).
 * No event is reported if the listener was not added since it was already registered.
 */
#define J9HOOK_REGISTRATION_EVENT 0
typedef struct J9HookRegistrationEvent {
	uintptr_t eventNum;
	J9HookFunction function;
	void *userData;
	uintptr_t isRegistration; /* true for register, false for unregister */
	uintptr_t agentID;
} J9HookRegistrationEvent;



#ifdef __cplusplus
}
#endif

#endif /* OMRHOOKABLE_H */
