/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#include "OMR_Agent.hpp"

#include <string.h>

#include "omrport.h"
#include "omragent_internal.h"

static OMR_ThreadAPI const theOmrThreadAPI = {
	omrthread_create,
	omrthread_monitor_init_with_name,
	omrthread_monitor_destroy,
	omrthread_monitor_enter,
	omrthread_monitor_exit,
	omrthread_monitor_wait,
	omrthread_monitor_notify_all,
	omrthread_attr_init,
	omrthread_attr_destroy,
	omrthread_attr_set_detachstate,
	omrthread_attr_set_category,
	omrthread_create_ex,
	omrthread_join
};

OMR_TI const OMR_Agent::theOmrTI = {
	OMR_TI_VERSION_0,
	(void *)&theOmrThreadAPI,
	omrtiBindCurrentThread,
	omrtiUnbindCurrentThread,
	omrtiRegisterRecordSubscriber,
	omrtiDeregisterRecordSubscriber,
	omrtiGetTraceMetadata,
	omrtiSetTraceOptions,
	omrtiGetSystemCpuLoad,
	omrtiGetProcessCpuLoad,
	omrtiGetMemoryCategories,
	omrtiFlushTraceData,
	omrtiGetFreePhysicalMemorySize,
	omrtiGetProcessVirtualMemorySize,
	omrtiGetProcessPrivateMemorySize,
	omrtiGetProcessPhysicalMemorySize,
	omrtiGetMethodDescriptions,
	omrtiGetMethodProperties
};

extern "C" OMR_Agent *
omr_agent_create(OMR_VM *vm, char const *arg)
{
	return OMR_Agent::createAgent(vm, arg);
}

extern "C" void
omr_agent_destroy(OMR_Agent *agent)
{
	OMR_Agent::destroyAgent(agent);
}

extern "C" omr_error_t
omr_agent_openLibrary(OMR_Agent *agent)
{
	return agent->openLibrary();
}

extern "C" omr_error_t
omr_agent_callOnLoad(OMR_Agent *agent)
{
	return agent->callOnLoad();
}

extern "C" omr_error_t
omr_agent_callOnUnload(OMR_Agent *agent)
{
	return agent->callOnUnload();
}

extern "C" OMR_TI const *
omr_agent_getTI(void)
{
	return &OMR_Agent::theOmrTI;
}

OMR_Agent::OMR_Agent(OMR_VM *vm, char const *arg) :
	_handle(0),
	_vm(vm),
	_onload(NULL),
	_onunload(NULL),
	_dllpath(NULL),
	_options(NULL),
	_state(UNINITIALIZED)
{
	OMRPORT_ACCESS_FROM_OMRVM(_vm);

	_agentCallbacks.version = 0;
	_agentCallbacks.onPreFork = onPreForkDefault;
	_agentCallbacks.onPostForkParent = onPostForkParentDefault;
	_agentCallbacks.onPostForkChild = onPostForkChildDefault;

	size_t len = strlen(arg) + 1;
	_dllpath = (char const *)omrmem_allocate_memory(len, OMRMEM_CATEGORY_VM);
	if (NULL != _dllpath) {
		memcpy((void *)_dllpath, arg, len);

		char *equals = strchr((char *)_dllpath, '=');
		if (NULL != equals) {
			*equals = '\0'; /* split the options from the dllpath */
			if ('\0' != equals[1]) {
				_options = &equals[1];
			}
		}
		_state = INITIALIZED;
	}
}

OMR_Agent *
OMR_Agent::createAgent(OMR_VM *vm, char const *arg)
{
	OMR_Agent *newAgent = NULL;

	if (NULL != arg) {
		OMRPORT_ACCESS_FROM_OMRVM(vm);
		newAgent = (OMR_Agent *)omrmem_allocate_memory(sizeof(*newAgent), OMRMEM_CATEGORY_VM);
		if (NULL != newAgent) {
			new(newAgent) OMR_Agent(vm, arg);
			if (INITIALIZED != newAgent->_state) {
				destroyAgent(newAgent);
				newAgent = NULL;
			}
		}
	}
	return newAgent;
}

void
OMR_Agent::destroyAgent(OMR_Agent *agent)
{
	if (NULL != agent) {
		OMRPORT_ACCESS_FROM_OMRVM(agent->_vm);
		if ((0 != agent->_handle) && (ONUNLOAD_SUCCESS == agent->_state)) {
			omrsl_close_shared_library(agent->_handle);
		}
		omrmem_free_memory((void *)agent->_dllpath);
		omrmem_free_memory((void *)agent);
	}
}

omr_error_t
OMR_Agent::openLibrary(void)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if (INITIALIZED != _state) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		uintptr_t portRc = 0;
		OMRPORT_ACCESS_FROM_OMRVM(_vm);

		portRc = omrsl_open_shared_library((char *)_dllpath, &_handle, OMRPORT_SLOPEN_DECORATE | OMRPORT_SLOPEN_LAZY);
		if (0 != portRc) {
			_state = OPEN_LIBRARY_ERROR;
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
		}

		if (OMR_ERROR_NONE == rc) {
			portRc = omrsl_lookup_name(_handle, (char *)"OMRAgent_OnLoad", (uintptr_t *)&_onload, (char *)"IPPPP");
			if (0 != portRc) {
				_state = LOOKUP_ONLOAD_ERROR;
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			}
		}

		if (OMR_ERROR_NONE == rc) {
			portRc = omrsl_lookup_name(_handle, (char *)"OMRAgent_OnUnload", (uintptr_t *)&_onunload, (char *)"IPP");
			if (0 != portRc) {
				_state = LOOKUP_ONUNLOAD_ERROR;
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			}
		}

		if (OMR_ERROR_NONE == rc) {
			_state = LIBRARY_OPENED;
		}
	}
	return rc;
}

omr_error_t
OMR_Agent::callOnLoad(void)
{
	omr_error_t rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	if (LIBRARY_OPENED == _state) {
		rc = _onload(&theOmrTI, _vm, _options, &_agentCallbacks);
		_state = ONLOAD_COMPLETED;
	}
	return rc;
}

omr_error_t
OMR_Agent::callOnUnload(void)
{
	omr_error_t rc = OMR_ERROR_INTERNAL;
	if (ONLOAD_COMPLETED == _state) {
		rc = _onunload(&theOmrTI, _vm);
		if (OMR_ERROR_NONE == rc) {
			_state = ONUNLOAD_SUCCESS;
		}
	}
	return rc;
}

omr_error_t
OMR_Agent::callOnPreFork(void)
{
	return _agentCallbacks.onPreFork();
}

omr_error_t
OMR_Agent::callOnPostForkParent(void)
{
	return _agentCallbacks.onPostForkParent();
}

omr_error_t
OMR_Agent::callOnPostForkChild(void)
{
	return _agentCallbacks.onPostForkChild();
}

omr_error_t
OMR_Agent::onPreForkDefault(void)
{
	return OMR_ERROR_NONE;
}

omr_error_t
OMR_Agent::onPostForkParentDefault(void)
{
	return OMR_ERROR_NONE;
}

omr_error_t
OMR_Agent::onPostForkChildDefault(void)
{
	return OMR_ERROR_NONE;
}
