/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
#include "omrport.h"
#include "hookable_api.h"
#include "hooksample_internal.h"

static int32_t testHookInterface(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface);
static void testEnabled(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t expectedResult);
static void testDisable(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t expectedResult);
static void testReserve(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t expectedResult);
static void testRegister(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t expectedResult);
static void testRegisterWithAgent(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t agentID, uintptr_t userData, uintptr_t expectedResult);
static void testUnregister(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event);
static void testUnregisterWithAgent(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t userData);
static void testDispatch(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, uintptr_t event, uintptr_t expectedResult);
static uintptr_t testAllocateAgentID(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface);
static void hookNormalEvent(J9HookInterface **hook, uintptr_t eventNum, void *voidEventData, void *userData);
static void hookOrderedEvent(J9HookInterface **hook, uintptr_t eventNum, void *voidEventData, void *userData);

static SampleHookInterface sampleHookInterface;

int32_t
verifyHookable(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount)
{
	int32_t rc = 0;
	J9HookInterface **hookInterface = J9_HOOK_INTERFACE(sampleHookInterface);
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	omrtty_printf("Testing hookable interface...\n");

	if (J9HookInitializeInterface(hookInterface, portLib, sizeof(sampleHookInterface))) {
		(*failCount)++;
		rc = -1;
	} else {
		(*passCount)++;
		rc = testHookInterface(portLib, passCount, failCount, hookInterface);

		(*hookInterface)->J9HookShutdownInterface(hookInterface);
	}

	omrtty_printf("Finished testing hookable interface.\n");

	return rc;
}

static int32_t
testHookInterface(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface)
{
	int32_t rc = 0;
	uintptr_t agent1, agent2, agent2andAHalf, agent3;

	/* all events should be enabled initially */
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT1, TRUE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT2, TRUE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, TRUE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT4, TRUE);

	/* disable event1 */
	testDisable(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT1, 0);
	testReserve(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT1, -1);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT1, FALSE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT2, TRUE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, TRUE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT4, TRUE);

	/* reserve event2 */
	testReserve(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT2, 0);
	testDisable(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT2, -1);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT1, FALSE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT2, TRUE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, TRUE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT4, TRUE);

	/* register event3 */
	testRegister(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, 0);
	testDisable(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, -1);
	testReserve(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, 0);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT1, FALSE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT2, TRUE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, TRUE);
	testEnabled(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT4, TRUE);

	/* register event2 (which was previously reserved) twice */
	/* (the second register should have no effect) */
	testRegister(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT2, 0);
	testRegister(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT2, 0);

	/* trigger an event */
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT1, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT2, 1);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT3, 1);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT4, 0);

	/* unregister the twice hooked event */
	testUnregister(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT2);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT1, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT2, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT3, 1);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT4, 0);

	/* unregister the once hooked event */
	testUnregister(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT1, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT2, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT3, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT4, 0);

	/* allocate some agent IDs */
	agent1 = testAllocateAgentID(portLib, passCount, failCount, hookInterface);
	agent2 = testAllocateAgentID(portLib, passCount, failCount, hookInterface);
	agent2andAHalf = testAllocateAgentID(portLib, passCount, failCount, hookInterface);
	agent3 = testAllocateAgentID(portLib, passCount, failCount, hookInterface);

	/* register the agents in order */
	testRegisterWithAgent(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT4, agent1, 1, 0);
	testRegisterWithAgent(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT4, agent2, 2, 0);
	testRegisterWithAgent(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT4, agent3, 3, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT4, 3);

	/* register the agents in reverse order */
	testRegisterWithAgent(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, agent3, 3, 0);
	testRegisterWithAgent(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, agent2, 2, 0);
	testRegisterWithAgent(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, agent1, 1, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT3, 3);

	/* unregister an agent */
	testUnregisterWithAgent(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, 2);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT3, 2);

	/* register it again with another agent in the same list position*/
	testRegisterWithAgent(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, agent2andAHalf, 2, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT3, 3);

	/* register a first chance istener */
	testRegisterWithAgent(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, J9HOOK_AGENTID_FIRST, 0, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT3, 4);

	/* and a last chance listener */
	testRegisterWithAgent(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, J9HOOK_AGENTID_LAST, 4, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT3, 5);

	/* registering a duplicate listener should have no effect, even if it's a different agent */
	testRegisterWithAgent(portLib, passCount, failCount, hookInterface, TESTHOOK_EVENT3, agent2, 3, 0);
	testDispatch(portLib, passCount, failCount, TESTHOOK_EVENT3, 5);

	return rc;
}

static void
testEnabled(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t expectedResult)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	if (((*hookInterface)->J9HookIsEnabled(hookInterface, event) == 0) == (expectedResult == 0)) {
		(*passCount)++;
	} else {
		omrtty_printf("Hook 0x%zx is %s. It should be %s.\n", event, (expectedResult ? "disabled" : "enabled"), (expectedResult ? "enabled" : "disabled"));
		(*failCount)++;
	}
}

static void
testDisable(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t expectedResult)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	if (((*hookInterface)->J9HookDisable(hookInterface, event) == 0) == (expectedResult == 0)) {
		(*passCount)++;
	} else {
		omrtty_printf("J9HookDisable for 0x%zx %s. It should have %s.\n", event, (expectedResult ? "succeeded" : "failed"), (expectedResult ? "failed" : "succeeded"));
		(*failCount)++;
	}
}

static void
testReserve(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t expectedResult)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	if (((*hookInterface)->J9HookReserve(hookInterface, event) == 0) == (expectedResult == 0)) {
		(*passCount)++;
	} else {
		omrtty_printf("J9HookReserve for 0x%zx %s. It should have %s.\n", event, (expectedResult ? "succeeded" : "failed"), (expectedResult ? "failed" : "succeeded"));
		(*failCount)++;
	}
}

static void
testRegister(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t expectedResult)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	if (((*hookInterface)->J9HookRegister(hookInterface, event, hookNormalEvent, NULL) == 0) == (expectedResult == 0)) {
		(*passCount)++;
	} else {
		omrtty_printf("J9HookRegister for 0x%zx %s. It should have %s.\n", event, (expectedResult ? "succeeded" : "failed"), (expectedResult ? "failed" : "succeeded"));
		(*failCount)++;
	}
}

static void
testRegisterWithAgent(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t agentID, uintptr_t userData, uintptr_t expectedResult)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	if (((*hookInterface)->J9HookRegister(hookInterface, event | J9HOOK_TAG_AGENT_ID, hookOrderedEvent, (void *)userData, agentID) == 0) == (expectedResult == 0)) {
		(*passCount)++;
	} else {
		omrtty_printf("J9HookRegister for 0x%zx %s. It should have %s.\n", event, (expectedResult ? "succeeded" : "failed"), (expectedResult ? "failed" : "succeeded"));
		(*failCount)++;
	}
}


static uintptr_t
testAllocateAgentID(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface)
{
	uintptr_t agentID = (*hookInterface)->J9HookAllocateAgentID(hookInterface);

	if (agentID != 0) {
		(*passCount)++;
	} else {
		OMRPORT_ACCESS_FROM_OMRPORT(portLib);

		omrtty_printf("J9HookAllocateAgentID failed.\n");
		(*failCount)++;
	}

	return agentID;
}

static void
testUnregister(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event)
{
	(*hookInterface)->J9HookUnregister(hookInterface, event, hookNormalEvent, NULL);
}

static void
testUnregisterWithAgent(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, J9HookInterface **hookInterface, uintptr_t event, uintptr_t userData)
{
	(*hookInterface)->J9HookUnregister(hookInterface, event, hookOrderedEvent, (void *)userData);
}

static void
testDispatch(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount, uintptr_t event, uintptr_t expectedResult)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	uintptr_t count = 0;

	switch (event) {
	case TESTHOOK_EVENT1:
		TRIGGER_TESTHOOK_EVENT1(sampleHookInterface, count, -1);
		break;
	case TESTHOOK_EVENT2:
		TRIGGER_TESTHOOK_EVENT2(sampleHookInterface, 1, count, -1);
		break;
	case TESTHOOK_EVENT3:
		TRIGGER_TESTHOOK_EVENT3(sampleHookInterface, 2, 3, count, -1);
		break;
	case TESTHOOK_EVENT4:
		TRIGGER_TESTHOOK_EVENT4(sampleHookInterface, 4, 5, 6, count, -1);
		break;
	}

	if (count == expectedResult) {
		(*passCount)++;
	} else {
		omrtty_printf("Incorrect number of listeners responded for 0x%zx. Got %d, expected %d\n", event, count, expectedResult);
		(*failCount)++;
	}
}

static void
hookNormalEvent(J9HookInterface **hook, uintptr_t eventNum, void *voidEventData, void *userData)
{
	uintptr_t increment = 1;

	if (J9_HOOK_INTERFACE(sampleHookInterface) != hook) {
		/* force the count to be wrong */
		increment = 8096;
	}

	switch (eventNum) {
	case TESTHOOK_EVENT1:
		((TestHookEvent1 *)voidEventData)->count += increment;
		break;
	case TESTHOOK_EVENT2:
		((TestHookEvent2 *)voidEventData)->count += increment;
		break;
	case TESTHOOK_EVENT3:
		((TestHookEvent3 *)voidEventData)->count += increment;
		break;
	case TESTHOOK_EVENT4:
		((TestHookEvent4 *)voidEventData)->count += increment;
		break;
	}

}

static void
hookOrderedEvent(J9HookInterface **hook, uintptr_t eventNum, void *voidEventData, void *userData)
{
	uintptr_t increment = 1;
	intptr_t prevAgent = 0;
	intptr_t thisAgent = (intptr_t)userData;

	if (J9_HOOK_INTERFACE(sampleHookInterface) != hook) {
		/* force the count to be wrong */
		increment = 8096;
	}

	switch (eventNum) {
	case TESTHOOK_EVENT1:
		prevAgent = ((TestHookEvent1 *)voidEventData)->prevAgent;
		((TestHookEvent1 *)voidEventData)->prevAgent = thisAgent;
		break;
	case TESTHOOK_EVENT2:
		prevAgent = ((TestHookEvent2 *)voidEventData)->prevAgent;
		((TestHookEvent2 *)voidEventData)->prevAgent = thisAgent;
		break;
	case TESTHOOK_EVENT3:
		prevAgent = ((TestHookEvent3 *)voidEventData)->prevAgent;
		((TestHookEvent3 *)voidEventData)->prevAgent = thisAgent;
		break;
	case TESTHOOK_EVENT4:
		prevAgent = ((TestHookEvent4 *)voidEventData)->prevAgent;
		((TestHookEvent4 *)voidEventData)->prevAgent = thisAgent;
		break;
	}

	if (prevAgent > thisAgent) {
		/* force the count to be wrong */
		increment = 42;
	}

	switch (eventNum) {
	case TESTHOOK_EVENT1:
		((TestHookEvent1 *)voidEventData)->count += increment;
		break;
	case TESTHOOK_EVENT2:
		((TestHookEvent2 *)voidEventData)->count += increment;
		break;
	case TESTHOOK_EVENT3:
		((TestHookEvent3 *)voidEventData)->count += increment;
		break;
	case TESTHOOK_EVENT4:
		((TestHookEvent4 *)voidEventData)->count += increment;
		break;
	}

}


