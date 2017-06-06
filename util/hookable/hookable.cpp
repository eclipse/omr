/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2017
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

#include <string.h>
#include <stdarg.h>
#include "pool_api.h"
#include "omrthread.h"
#include "omrhookable.h"
#include "omrmemcategories.h"
#include "omrutil.h"
#include "AtomicSupport.hpp"
#include "ut_j9hook.h"
#include "omrtrace.h"

extern "C" {

static void J9HookUnregister(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum, J9HookFunction function, void *userData);
static intptr_t J9HookRegister(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum, J9HookFunction function, void *userData, ...);
static intptr_t J9HookRegisterWithCallSite(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum, J9HookFunction function, const char *callsite, void *userData, ...);
static void J9HookShutdownInterface(struct J9HookInterface **hookInterface);
static intptr_t J9HookDisable(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum);
static intptr_t J9HookIsEnabled(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum);
static void J9HookDispatch(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum, void *eventData);
static intptr_t J9HookReserve(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum);
static uintptr_t J9HookAllocateAgentID(struct J9HookInterface **hookInterface);
static void J9HookDeallocateAgentID(struct J9HookInterface **hookInterface, uintptr_t agentID);

static J9CONST_TABLE J9HookInterface hookFunctionTable = {
	J9HookDispatch,
	J9HookDisable,
	J9HookReserve,
	J9HookRegister,
	J9HookRegisterWithCallSite,
	J9HookUnregister,
	J9HookShutdownInterface,
	J9HookIsEnabled,
	J9HookAllocateAgentID,
	J9HookDeallocateAgentID,
};

/* flags are stored at the beginning of the interface just after the common interface fields in ascending order */
#define HOOK_FLAGS(interface, event) (((uint8_t*)((interface) + 1))[event])

/* records are stored at the END of the interface in descending order */
#define HOOK_RECORD(interface, event) (((J9HookRecord**)( (uint8_t*)(interface) + (interface)->size ))[ -1 - (event)])

/* e.g.
 0: J9CommonInterface::interface
 4: J9CommonInterface::size
 8: J9CommonInterface::lock
 C: J9CommonInterface::pool
10: J9CommonInterface::nextAgentID
14: flags[0]
15: flags[1]
16: padding
17: padding
18: record[1]
1C: record[0]
*/

/* each record has an ID which is used to detect inconsistent records without resorting to monitors */
/* even IDs are valid. Odd IDs are invalid */
/* a record is consistent if the same ID is in the record before all the fields are read and after all the fields are read */
/* read and write barriers must be used to ensure that reads and writes occur in the correct order */
#define HOOK_INITIAL_ID (0)
#define HOOK_IS_VALID_ID(id) ( ((id) & 1) == 0)
#define HOOK_INVALID_ID(id) ((id) | 1)
#define HOOK_VALID_ID(id) ( (((id) | 1) + 1) )


intptr_t
omrhook_lib_control(const char *key, uintptr_t value)
{
	intptr_t rc = -1;

	if (0 != value) {
#if defined(OMR_RAS_TDF_TRACE)
		/* return value of 0 is success */
		if (0 == strcmp(J9HOOK_LIB_CONTROL_TRACE_START, key)) {
			UtInterface *utIntf = (UtInterface *)value;
			UT_MODULE_LOADED(utIntf);
			rc = 0;
		} else if (0 == strcmp(J9HOOK_LIB_CONTROL_TRACE_STOP, key)) {
			UtInterface *utIntf = (UtInterface *)value;
			UT_MODULE_UNLOADED(utIntf);
			rc = 0;
		}
#else
		rc = 0;
#endif /* OMR_RAS_TDF_TRACE */
	}
	return rc;
}
/*
 * Prepares the specified hook interface for first use.
 *
 * This function may be called directly.
 *
 * Returns 0 on success, non-zero on failure
 */
intptr_t
J9HookInitializeInterface(struct J9HookInterface **hookInterface, OMRPortLibrary *portLib, size_t interfaceSize)
{
	J9CommonHookInterface *commonInterface = (J9CommonHookInterface *)hookInterface;

	memset(commonInterface, 0, interfaceSize);

	commonInterface->hookInterface = (J9HookInterface *)GLOBAL_TABLE(hookFunctionTable);

	commonInterface->size = interfaceSize;

	if (omrthread_monitor_init_with_name(&commonInterface->lock, 0, "Hook Interface")) {
		J9HookShutdownInterface(hookInterface);
		return J9HOOK_ERR_NOMEM;
	}

	commonInterface->pool = pool_new(sizeof(J9HookRecord), 0, 0, 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT((OMRPortLibrary *)portLib));
	if (commonInterface->pool == NULL) {
		J9HookShutdownInterface(hookInterface);
		return J9HOOK_ERR_NOMEM;
	}

	commonInterface->nextAgentID = J9HOOK_AGENTID_DEFAULT + 1;
	commonInterface->portLib = portLib;
	commonInterface->threshold4Trace = OMRHOOK_DEFAULT_THRESHOLD_IN_MILLISECONDS_WARNING_CALLBACK_ELAPSED_TIME;

	commonInterface->eventSize = (interfaceSize - sizeof(J9CommonHookInterface)) / (sizeof(U_8) + sizeof(OMREventInfo4Dump) + sizeof(J9HookRecord*));

	return 0;
}

/*
 * Shuts down the specified hook interface.
 *
 * This function should not be called directly. It should be called through the hook interface
 */
static void
J9HookShutdownInterface(struct J9HookInterface **hookInterface)
{
	J9CommonHookInterface *commonInterface = (J9CommonHookInterface *)hookInterface;

	if (commonInterface->lock) {
		omrthread_monitor_destroy(commonInterface->lock);
	}

	if (commonInterface->pool) {
		pool_kill(commonInterface->pool);
	}
}


/*
 * Inform all registered listeners that the specified event has occurred. Details about the
 * event should be available through eventData.
 *
 * If the J9HOOK_TAG_ONCE bit is set in the taggedEventNum, then the event is disabled
 * before the listeners are informed. Any attempts to add listeners to a TAG_ONCE event
 * once it has been reported will fail.
 *
 * This function should not be called directly. It should be called through the hook interface
 *
 */
static void
J9HookDispatch(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum, void *eventData)
{
	uintptr_t eventNum = taggedEventNum & J9HOOK_EVENT_NUM_MASK;
	J9CommonHookInterface *commonInterface = (J9CommonHookInterface *)hookInterface;
	J9HookRecord *record = HOOK_RECORD(commonInterface, eventNum);

	if (taggedEventNum & J9HOOK_TAG_ONCE) {
		uint8_t oldFlags;

		omrthread_monitor_enter(commonInterface->lock);
		oldFlags = HOOK_FLAGS(commonInterface, eventNum);
		/* clear the HOOKED and RESERVED flags and set the DISABLED flag */
		HOOK_FLAGS(commonInterface, eventNum) = (oldFlags | J9HOOK_FLAG_DISABLED) & ~(J9HOOK_FLAG_RESERVED | J9HOOK_FLAG_HOOKED);
		omrthread_monitor_exit(commonInterface->lock);

		if (oldFlags & J9HOOK_FLAG_DISABLED) {
			/* already reported */
			return;
		}
	}

	while (record) {
		J9HookFunction function;
		void *userData;
		uintptr_t id;

		/* ensure that the id is read before any other fields */
		id = record->id;
		if (HOOK_IS_VALID_ID(id)) {
			VM_AtomicSupport::readBarrier();

			function = record->function;
			userData = record->userData;

			/* now read the id again to make sure that nothing has changed */
			VM_AtomicSupport::readBarrier();
			if (record->id == id) {
				uint64_t timeDelta = 0;
				OMRPORT_ACCESS_FROM_OMRPORT(commonInterface->portLib);
				uint64_t startTime = omrtime_current_time_millis();
				function(hookInterface, eventNum, eventData, userData);
				timeDelta = omrtime_current_time_millis() - startTime;

				OMREventInfo4Dump *eventDump = J9HOOK_DUMPINFO(commonInterface, eventNum);

				/* record hook info for dump if elapse time is longer than 1 millisecond */
				if ((NULL != eventDump) && (0 != timeDelta)) {
					eventDump->lastHook.callsite = record->callsite;
					eventDump->lastHook.func_ptr = (void *)record->function;
					eventDump->lastHook.startTime = startTime;
					eventDump->lastHook.duration = timeDelta;
					if (eventDump->longestHook.duration < eventDump->lastHook.duration) {
						eventDump->longestHook.callsite = eventDump->lastHook.callsite;
						eventDump->longestHook.startTime = eventDump->lastHook.startTime;
						eventDump->longestHook.func_ptr = eventDump->lastHook.func_ptr;
						eventDump->longestHook.duration = eventDump->lastHook.duration;
					}
				}

				if (commonInterface->threshold4Trace <= timeDelta) {
					const char *callsite = "UNKNOWN";
					char buffer[32];
					if (NULL != record->callsite) {
						callsite = record->callsite;
					} else {
						/* if the callsite info can not be retrieved, use callback function pointer instead  */
						omrstr_printf(buffer, sizeof(buffer), "0x%p", record->function);
						callsite = buffer;
					}
					Trc_Hook_Dispatch_Exceed_Threshold_Event(callsite, timeDelta);
				}
			} else {
				/* this record has been updated while we were reading it. Skip it. */
			}
		}

		record = record->next;
	}
}



/*
 * Mark this event as disabled. Any future attempts to add a hook for this event
 * will result in an error.
 *
 * This function should not be called directly. It should be called through the hook interface
 *
 * Returns 0 on success, or non-zero if the event is already hooked or reserved
 */
static intptr_t
J9HookDisable(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum)
{
	J9CommonHookInterface *commonInterface = (J9CommonHookInterface *)hookInterface;
	uintptr_t eventNum = taggedEventNum & J9HOOK_EVENT_NUM_MASK;

	/* try to answer without using the lock, first */
	if (HOOK_FLAGS(commonInterface, eventNum) & J9HOOK_FLAG_RESERVED) {
		return -1;
	} else if (HOOK_FLAGS(commonInterface, eventNum) & J9HOOK_FLAG_DISABLED) {
		return 0;
	} else {
		intptr_t rc = 0;

		omrthread_monitor_enter(commonInterface->lock);

		if (HOOK_FLAGS(commonInterface, eventNum) & (J9HOOK_FLAG_RESERVED | J9HOOK_FLAG_HOOKED)) {
			rc = -1;
		} else {
			HOOK_FLAGS(commonInterface, eventNum) |= J9HOOK_FLAG_DISABLED;
		}

		omrthread_monitor_exit(commonInterface->lock);

		return rc;
	}
}



/*
 * Use J9HookReserve to indicate your intention to hook an event in the future.
 * Certain events must be reserved before the providing module initializes completely,
 * or it may make decisions which make it impossible to add the hooks later.
 * Once the point of no-return is reached, the providing module will disable the
 * event if it has not been registered or reserved.
 *
 * This function should not be called directly. It should be called through the hook interface
 *
 * returns 0 on success
 * non-zero if the event has been disabled
 */
static intptr_t
J9HookReserve(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum)
{
	J9CommonHookInterface *commonInterface = (J9CommonHookInterface *)hookInterface;
	intptr_t rc = 0;
	uintptr_t eventNum = taggedEventNum & J9HOOK_EVENT_NUM_MASK;

	omrthread_monitor_enter(commonInterface->lock);

	if (HOOK_FLAGS(commonInterface, eventNum) & J9HOOK_FLAG_DISABLED) {
		rc = -1;
	} else {
		HOOK_FLAGS(commonInterface, eventNum) |= J9HOOK_FLAG_RESERVED;
	}

	omrthread_monitor_exit(commonInterface->lock);

	return rc;
}

static intptr_t
J9HookRegisterWithCallSitePrivate(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum, J9HookFunction function, const char *callsite, void *userData, uintptr_t agentID)
{
	J9CommonHookInterface *commonInterface = (J9CommonHookInterface *)hookInterface;
	J9HookRegistrationEvent eventStruct;
	intptr_t rc = 0;
	uintptr_t eventNum = taggedEventNum & J9HOOK_EVENT_NUM_MASK;

	omrthread_monitor_enter(commonInterface->lock);

	if (HOOK_FLAGS(commonInterface, eventNum) & J9HOOK_FLAG_DISABLED) {
		rc = -1;
	} else {
		J9HookRecord *insertionPoint = NULL;
		J9HookRecord *emptyRecord = NULL;
		J9HookRecord *record = HOOK_RECORD(commonInterface, eventNum);
		/* at the end of the loop, insertionPoint will point to the last record which should be triggered before the one we're adding */
		while (record) {
			if ((taggedEventNum & J9HOOK_TAG_REVERSE_ORDER) ? record->agentID >= agentID : record->agentID <= agentID) {
				insertionPoint = record;
			}
			if (!HOOK_IS_VALID_ID(record->id)) {
				if ((taggedEventNum & J9HOOK_TAG_REVERSE_ORDER) ? record->agentID >= agentID : record->agentID <= agentID) {
					emptyRecord = record;
				}
			} else if ((record->function == function) && (record->userData == userData)) {
				/* this listener is already registered */
				++(record->count);
				omrthread_monitor_exit(commonInterface->lock);
				return 0;
			}
			record = record->next;
		}

		/*
		 * Re-use the empty record if it is in a legitimate position for the requested agent.
		 * (It is a legitimate position if all records before this position have the same or lower agent IDs (tested previously)
		 * and all records after this position have the same or higher IDs)
		 */
		if ((emptyRecord != NULL)
			&& ((emptyRecord->next == NULL)
				|| ((taggedEventNum & J9HOOK_TAG_REVERSE_ORDER) ?
					(emptyRecord->next->agentID <= agentID) :
					(emptyRecord->next->agentID >= agentID)))
		) {
			emptyRecord->function = function;
			emptyRecord->callsite = callsite;
			emptyRecord->userData = userData;
			emptyRecord->count = 1;
			emptyRecord->agentID = agentID;

			VM_AtomicSupport::writeBarrier();

			emptyRecord->id = HOOK_VALID_ID(emptyRecord->id);

			HOOK_FLAGS(commonInterface, eventNum) |= J9HOOK_FLAG_HOOKED | J9HOOK_FLAG_RESERVED;
		} else {
			record = (J9HookRecord *)pool_newElement(commonInterface->pool);
			if (record == NULL) {
				rc = -1;
			} else {
				if (insertionPoint == NULL) {
					record->next = HOOK_RECORD(commonInterface, eventNum);
				} else {
					record->next = insertionPoint->next;
				}
				record->function = function;
				record->callsite = callsite;
				record->userData = userData;
				record->count = 1;
				record->id = HOOK_INITIAL_ID;
				record->agentID = agentID;

				VM_AtomicSupport::writeBarrier();

				if (insertionPoint == NULL) {
					HOOK_RECORD(commonInterface, eventNum) = record;
				} else {
					insertionPoint->next = record;
				}

				HOOK_FLAGS(commonInterface, eventNum) |= J9HOOK_FLAG_HOOKED | J9HOOK_FLAG_RESERVED;
			}
		}
	}

	omrthread_monitor_exit(commonInterface->lock);

	/* report the registration event */
	eventStruct.eventNum = eventNum;
	eventStruct.function = function;
	eventStruct.userData = userData;
	eventStruct.isRegistration = 1;
	eventStruct.agentID = agentID;
	(*hookInterface)->J9HookDispatch(hookInterface, J9HOOK_REGISTRATION_EVENT, &eventStruct);

	return rc;
}

/*
 * Register a listener for the specified event.
 *
 * If a listener with the same function and userData is already registered, no action is taken,
 * and 0 is returned (indicating success). Registrations are NOT counted. Even if a listener
 * is registered twice, it will be removed the first time it is unregistered.
 *
 * If the J9HOOK_TAG_AGENT_ID is set in taggedEventNum, the first var-args argument must be
 * a uintptr_t agent ID. If the bit is not set, the listener will be added to the J9HOOK_AGENTID_DEFAULT
 * agent. The use of an agent ID allows more control over the order in which events are reported.
 * Listeners with lower agent IDs will always receive events before listeners with higher agent IDs.
 * The special J9HOOK_AGENT_FIRST and J9HOOK_AGENT_LAST IDs may be used to register
 * listeners which will be among the first or last to receive an event.
 *
 * This function should not be called directly. It should be called through the hook interface
 *
 * Returns 0 on success,
 * J9HOOK_ERR_DISABLED if the event has been disabled
 * J9HOOK_ERR_NOMEM if insufficient resources exist to register the listener
 * J9HOOK_ERR_INVALID_AGENT_ID if the optional agent ID is invalid
 */
static intptr_t
J9HookRegister(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum, J9HookFunction function, void *userData, ...)
{
	uintptr_t agentID = J9HOOK_AGENTID_DEFAULT;

	if (taggedEventNum & J9HOOK_TAG_AGENT_ID) {
		va_list args;

		va_start(args, userData);
		agentID = va_arg(args, uintptr_t);
		va_end(args);
	}
	return J9HookRegisterWithCallSitePrivate(hookInterface, taggedEventNum, function, NULL, userData, agentID);
}

static intptr_t
J9HookRegisterWithCallSite(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum, J9HookFunction function, const char *callsite, void *userData, ...)
{
	uintptr_t agentID = J9HOOK_AGENTID_DEFAULT;

	if (taggedEventNum & J9HOOK_TAG_AGENT_ID) {
		va_list args;

		va_start(args, userData);
		agentID = va_arg(args, uintptr_t);
		va_end(args);
	}
	return J9HookRegisterWithCallSitePrivate(hookInterface, taggedEventNum, function, callsite, userData, agentID);
}


/*
 * Remove the specified function from the listeners for eventNum.
 * If userData is NULL, all functions matching function are removed.
 * If userData is non-NULL, only functions with matching userData are removed.
 *
 * This function should not be called directly. It should be called through the hook interface
 */
static void
J9HookUnregister(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum, J9HookFunction function, void *userData)
{
	J9CommonHookInterface *commonInterface = (J9CommonHookInterface *)hookInterface;
	J9HookRecord *record;
	J9HookRegistrationEvent eventStruct;
	uintptr_t hooksRemaining = 0;
	uintptr_t hooksRemoved = 0;
	uintptr_t eventNum = taggedEventNum & J9HOOK_EVENT_NUM_MASK;

	eventStruct.eventNum = eventNum;
	eventStruct.function = function;
	eventStruct.isRegistration = 0;
	eventStruct.userData = NULL;
	eventStruct.agentID = J9HOOK_AGENTID_DEFAULT;

	omrthread_monitor_enter(commonInterface->lock);

	record = HOOK_RECORD(commonInterface, eventNum);
	while (record) {
		if (record->function == function) {
			if ((userData == NULL) || (record->userData == userData)) {
				if (taggedEventNum & J9HOOK_TAG_COUNTED) {
					if (--(record->count) != 0) {
						omrthread_monitor_exit(commonInterface->lock);
						return;
					}
				}

				if (userData != NULL) {
					/* copy data from the event record so that we can report it once we've released the mutex */
					eventStruct.userData = record->userData;
					eventStruct.agentID = record->agentID;
				}

				/* mark the record as invalid so that it can be recycled */
				record->id = HOOK_INVALID_ID(record->id);
				hooksRemoved++;
			}
		}
		if (HOOK_IS_VALID_ID(record->id)) {
			hooksRemaining++;
		}
		record = record->next;
	}

	if (hooksRemaining == 0) {
		HOOK_FLAGS(commonInterface, eventNum) &= ~J9HOOK_FLAG_HOOKED;
	}

	omrthread_monitor_exit(commonInterface->lock);

	if (hooksRemoved != 0) {
		/* report the unregistration event */
		(*hookInterface)->J9HookDispatch(hookInterface, J9HOOK_REGISTRATION_EVENT, &eventStruct);
	}
}

/**
 * Determine if the specified event has been disabled. This function does not modify the
 * hook interface in any way. Note that there's no atomic protection of any kind -- just
 * because the event wasn't disabled when you called this function doesn't mean that it
 * won't be by the time you try to register for it.
 *
 * @return 0 (false) if the event is disabled, non-zero (true) if the event is not disabled.
 */
static intptr_t
J9HookIsEnabled(struct J9HookInterface **hookInterface, uintptr_t taggedEventNum)
{
	J9CommonHookInterface *commonInterface = (J9CommonHookInterface *)hookInterface;
	uintptr_t eventNum = taggedEventNum & J9HOOK_EVENT_NUM_MASK;

	if (HOOK_FLAGS(commonInterface, eventNum) & J9HOOK_FLAG_DISABLED) {
		return 0;
	}

#if 0
	/* at some point, we may wish to distinguish between 'not disabled' and reserved. */
	if (HOOK_FLAGS(commonInterface, eventNum) & J9HOOK_FLAG_RESERVED) {
		return 2;
	}
#endif

	return 1;
}

/**
 * @return a new unique identifier for a hookable agent, or 0 on failure
 *
 * This function should not be called directly. It should be called through the hook interface
 */
static uintptr_t
J9HookAllocateAgentID(struct J9HookInterface **hookInterface)
{
	J9CommonHookInterface *commonInterface = (J9CommonHookInterface *)hookInterface;
	uintptr_t id = 0;

	omrthread_monitor_enter(commonInterface->lock);
	if (commonInterface->nextAgentID < J9HOOK_AGENTID_LAST) {
		id = commonInterface->nextAgentID++;
	}
	omrthread_monitor_exit(commonInterface->lock);

	return id;
}

/**
 * Deallocate the specified agentID.
 *
 * This function should not be called directly. It should be called through the hook interface
 *
 * @note the initial implementation will not support deallocation of IDs.
 * This API is provided in case a future implementation chooses to support
 * this.
 */
static void
J9HookDeallocateAgentID(struct J9HookInterface **hookInterface, uintptr_t agentID)
{
	return;
}

}
