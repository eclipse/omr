/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#include "AllocateDescription.hpp"
#include "AllocationStats.hpp"
#include "Dispatcher.hpp"
#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "CollectionStatistics.hpp"
#include "ObjectAllocationInterface.hpp"
#include "VerboseHandlerOutput.hpp"
#include "VerboseManager.hpp"
#include "VerboseWriterChain.hpp"

#include "gcutils.h"

static void verboseHandlerInitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerHeapResize(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);

MM_VerboseHandlerOutput *
MM_VerboseHandlerOutput::newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager)
{
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());

	MM_VerboseHandlerOutput *verboseHandlerOutput = (MM_VerboseHandlerOutput*)extensions->getForge()->allocate(sizeof(MM_VerboseHandlerOutput), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != verboseHandlerOutput) {
		new(verboseHandlerOutput) MM_VerboseHandlerOutput(extensions);
		if(!verboseHandlerOutput->initialize(env, manager)) {
			verboseHandlerOutput->kill(env);
			verboseHandlerOutput = NULL;
		}
	}
	return verboseHandlerOutput;
}


MM_VerboseHandlerOutput::MM_VerboseHandlerOutput(MM_GCExtensionsBase *extensions) :
	_extensions(extensions)
	,_omrVM(NULL)
	,_mmPrivateHooks(NULL)
	,_mmOmrHooks(NULL)
	,_manager(NULL)
{};

bool
MM_VerboseHandlerOutput::initialize(MM_EnvironmentBase *env, MM_VerboseManager *manager)
{
	_omrVM = env->getOmrVM();
	_mmPrivateHooks = J9_HOOK_INTERFACE(_extensions->privateHookInterface);
	_mmOmrHooks = J9_HOOK_INTERFACE(_extensions->omrHookInterface);
	_manager = manager;

	return true;
}

void
MM_VerboseHandlerOutput::kill(MM_EnvironmentBase *env)
{
	tearDown(env);

	_extensions->getForge()->free(this);
}

void
MM_VerboseHandlerOutput::tearDown(MM_EnvironmentBase *env)
{
	return ;
}

void
MM_VerboseHandlerOutput::enableVerbose()
{
	/* Initialized */
	(*_mmOmrHooks)->J9HookRegister(_mmOmrHooks, J9HOOK_MM_OMR_INITIALIZED, verboseHandlerInitialized, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_HEAP_RESIZE, verboseHandlerHeapResize, (void *)this);

	return ;
}

void
MM_VerboseHandlerOutput::disableVerbose()
{
	/* Initialized */
	(*_mmOmrHooks)->J9HookUnregister(_mmOmrHooks, J9HOOK_MM_OMR_INITIALIZED, verboseHandlerInitialized, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_HEAP_RESIZE, verboseHandlerHeapResize, NULL);

	return ;
}

bool
MM_VerboseHandlerOutput::getThreadName(char *buf, uintptr_t bufLen, OMR_VMThread *vmThread)
{
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	omrstr_printf(buf, bufLen, "OMR_VMThread [%]",vmThread);

	return true;
}

void
MM_VerboseHandlerOutput::writeVmArgs(MM_EnvironmentBase* env)
{
	/* TODO (stefanbu) OMR does not support argument parsing yet, but we should repsect schema.*/
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	writer->formatAndOutput(env, 1, "<vmargs>");
	writer->formatAndOutput(env, 1, "</vmargs>");
}


uintptr_t
MM_VerboseHandlerOutput::getTagTemplate(char *buf, uintptr_t bufsize, uint64_t wallTimeMs)
{
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	uintptr_t bufPos = 0;
	bufPos += omrstr_printf(buf, bufsize, "timestamp=\"");
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_PRE_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "%03llu", wallTimeMs % 1000);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_POST_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "\"");

	return bufPos;
}

uintptr_t
MM_VerboseHandlerOutput::getTagTemplate(char *buf, uintptr_t bufsize, uintptr_t id, uint64_t wallTimeMs)
{
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	uintptr_t bufPos = 0;
	bufPos += omrstr_printf(buf, bufsize, "id=\"%zu\" timestamp=\"", id);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_PRE_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "%03llu", wallTimeMs % 1000);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_POST_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "\"");

	return bufPos;
}

uintptr_t
MM_VerboseHandlerOutput::getTagTemplate(char *buf, uintptr_t bufsize, uintptr_t id, const char *type, uintptr_t contextId, uint64_t wallTimeMs)
{
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	uintptr_t bufPos = 0;
	bufPos += omrstr_printf(buf, bufsize, "id=\"%zu\" type=\"%s\" contextid=\"%zu\" timestamp=\"", id, type, contextId);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_PRE_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "%03llu", wallTimeMs % 1000);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_POST_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "\"");

	return bufPos;
}

uintptr_t
MM_VerboseHandlerOutput::getTagTemplateWithOldType(char *buf, uintptr_t bufsize, uintptr_t id, const char *oldType, const char *newType, uintptr_t contextId, uint64_t wallTimeMs)
{
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	uintptr_t bufPos = 0;
	bufPos += omrstr_printf(buf, bufsize, "id=\"%zu\" oldtype=\"%s\" newtype=\"%s\" contextid=\"%zu\" timestamp=\"", id, oldType, newType, contextId);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_PRE_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "%03llu", wallTimeMs % 1000);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_POST_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "\"");

	return bufPos;
}

uintptr_t
MM_VerboseHandlerOutput::getTagTemplate(char *buf, uintptr_t bufsize, uintptr_t id, const char *type, uintptr_t contextId, uint64_t timeus, uint64_t wallTimeMs)
{
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	uintptr_t bufPos = 0;
	bufPos += omrstr_printf(buf, bufsize, "id=\"%zu\" type=\"%s\" timems=\"%llu.%03.3llu\" contextid=\"%zu\" timestamp=\"", id, type, timeus / 1000, timeus % 1000, contextId);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_PRE_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "%03llu", wallTimeMs % 1000);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_POST_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "\"");

	return bufPos;
}

uintptr_t
MM_VerboseHandlerOutput::getTagTemplateWithDuration(char *buf, uintptr_t bufsize, uintptr_t id, const char *type, uintptr_t contextId, uint64_t durationus, uint64_t usertimeus, uint64_t cputimeus, uint64_t wallTimeMs)
{
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	uintptr_t bufPos = 0;
	bufPos += omrstr_printf(buf, bufsize, "id=\"%zu\" type=\"%s\" contextid=\"%zu\" durationms=\"%llu.%03.3llu\" usertimems=\"%llu.%03.3llu\" systemtimems=\"%llu.%03.3llu\" timestamp=\"",
 			id, type, contextId, durationus / 1000, durationus % 1000, usertimeus / 1000, usertimeus % 1000, cputimeus / 1000, cputimeus % 1000);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_PRE_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "%03llu", wallTimeMs % 1000);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_POST_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "\"");

	return bufPos;
}

void
MM_VerboseHandlerOutput::handleInitializedInnerStanzas(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	return;
}

void
MM_VerboseHandlerOutput::handleInitializedRegion(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_InitializedEvent* event = (MM_InitializedEvent*)eventData;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	writer->formatAndOutput(env, 1, "<region>");
	writer->formatAndOutput(env, 2, "<attribute name=\"regionSize\" value=\"%zu\" />", event->regionSize);
	writer->formatAndOutput(env, 2, "<attribute name=\"regionCount\" value=\"%zu\" />", event->regionCount);
#if defined(OMR_GC_ARRAYLETS)
	writer->formatAndOutput(env, 2, "<attribute name=\"arrayletLeafSize\" value=\"%zu\" />", event->arrayletLeafSize);
#endif	
	writer->formatAndOutput(env, 1, "</region>");
}

void
MM_VerboseHandlerOutput::handleInitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_InitializedEvent* event = (MM_InitializedEvent*)eventData;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	char tagTemplate[200];

	_manager->setInitializedTime(event->timestamp);

	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), omrtime_current_time_millis());
	enterAtomicReportingBlock();
	writer->formatAndOutput(env, 0, "<initialized %s>", tagTemplate);
	writer->formatAndOutput(env, 1, "<attribute name=\"gcPolicy\" value=\"%s\" />", event->gcPolicy);
	writer->formatAndOutput(env, 1, "<attribute name=\"maxHeapSize\" value=\"0x%zx\" />", event->maxHeapSize);
	writer->formatAndOutput(env, 1, "<attribute name=\"initialHeapSize\" value=\"0x%zx\" />", event->initialHeapSize);
#if defined(OMR_GC_COMPRESSED_POINTERS)
	writer->formatAndOutput(env, 1, "<attribute name=\"compressedRefs\" value=\"true\" />");
	writer->formatAndOutput(env, 1, "<attribute name=\"compressedRefsDisplacement\" value=\"0x%zx\" />", 0);
	writer->formatAndOutput(env, 1, "<attribute name=\"compressedRefsShift\" value=\"0x%zx\" />", event->compressedPointersShift);
#else /* defined(OMR_GC_COMPRESSED_POINTERS) */
	writer->formatAndOutput(env, 1, "<attribute name=\"compressedRefs\" value=\"false\" />");
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	writer->formatAndOutput(env, 1, "<attribute name=\"pageSize\" value=\"0x%zx\" />", event->heapPageSize);
	writer->formatAndOutput(env, 1, "<attribute name=\"pageType\" value=\"%s\" />", event->heapPageType);
	writer->formatAndOutput(env, 1, "<attribute name=\"requestedPageSize\" value=\"0x%zx\" />", event->heapRequestedPageSize);
	writer->formatAndOutput(env, 1, "<attribute name=\"requestedPageType\" value=\"%s\" />", event->heapRequestedPageType);
	writer->formatAndOutput(env, 1, "<attribute name=\"gcthreads\" value=\"%zu\" />", event->gcThreads);
	writer->formatAndOutput(env, 1, "<attribute name=\"numaNodes\" value=\"%zu\" />", event->numaNodes);

	handleInitializedInnerStanzas(hook, eventNum, eventData);

	writer->formatAndOutput(env, 1, "<system>");
	writer->formatAndOutput(env, 2, "<attribute name=\"physicalMemory\" value=\"%llu\" />", event->physicalMemory);
	writer->formatAndOutput(env, 2, "<attribute name=\"numCPUs\" value=\"%zu\" />", event->numCPUs);
	writer->formatAndOutput(env, 2, "<attribute name=\"architecture\" value=\"%s\" />", event->architecture);
	writer->formatAndOutput(env, 2, "<attribute name=\"os\" value=\"%s\" />", event->os);
	writer->formatAndOutput(env, 2, "<attribute name=\"osVersion\" value=\"%s\" />", event->osVersion);
	writer->formatAndOutput(env, 1, "</system>");
	
	writeVmArgs(env);

	writer->formatAndOutput(env, 0, "</initialized>\n");
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleCycleStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_GCCycleStartEvent* event = (MM_GCCycleStartEvent*)eventData;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->omrVMThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	uint64_t currentTime = event->timestamp;
	uint64_t previousTime = 0;

	switch (env->_cycleState->_type) {
	case OMR_GC_CYCLE_TYPE_DEFAULT:
		break;
	case OMR_GC_CYCLE_TYPE_GLOBAL:
		previousTime = _manager->getLastGlobalGCTime();
		_manager->setLastGlobalGCTime(currentTime);
		break;
	case OMR_GC_CYCLE_TYPE_SCAVENGE:
		previousTime = _manager->getLastLocalGCTime();
		_manager->setLastLocalGCTime(currentTime);
		break;
	case OMR_GC_CYCLE_TYPE_VLHGC_PARTIAL_GARBAGE_COLLECT:
		previousTime = _manager->getLastPartialGCTime();
		_manager->setLastPartialGCTime(currentTime);
		break;
	case OMR_GC_CYCLE_TYPE_VLHGC_GLOBAL_MARK_PHASE:
		previousTime = _manager->getLastGlobalMarkGCTime();
		_manager->setLastGlobalMarkGCTime(currentTime);
		break;
	case OMR_GC_CYCLE_TYPE_VLHGC_GLOBAL_GARBAGE_COLLECT:
		previousTime = _manager->getLastBalancedGlobalGCTime();
		_manager->setLastBalancedGlobalGCTime(currentTime);
		break;
	}

	if (0 == previousTime) {
		previousTime = _manager->getInitializedTime();
	}

	uint64_t deltaTime = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&deltaTime, previousTime, currentTime);

	const char* cycleType = getCurrentCycleType(env);
	char tagTemplate[200];
	uintptr_t id = _manager->getIdAndIncrement();
	env->_cycleState->_verboseContextID = id;
	getTagTemplate(tagTemplate, sizeof(tagTemplate), id, cycleType, 0 /* Needs context id */, omrtime_current_time_millis());

	enterAtomicReportingBlock();
	if (!deltaTimeSuccess) {
		writer->formatAndOutput(env, 0, "<warning details=\"clock error detected, following timing may be inaccurate\" />");
	}
	if(hasCycleStartInnerStanzas()) {
		writer->formatAndOutput(env, 0, "<cycle-start %s intervalms=\"%llu.%03llu\">", tagTemplate, deltaTime / 1000 , deltaTime % 1000);
		handleCycleStartInnerStanzas(hook, eventNum, eventData, 1);
		writer->formatAndOutput(env, 0, "</cycle-start>");
	} else {
		writer->formatAndOutput(env, 0, "<cycle-start %s intervalms=\"%llu.%03llu\" />", tagTemplate, deltaTime / 1000 , deltaTime % 1000);
	}
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleCycleContinue(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_GCCycleContinueEvent* event = (MM_GCCycleContinueEvent*)eventData;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->omrVMThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	const char* newCycleType = getCurrentCycleType(env);
	const char* oldCycleType = getCycleType(event->oldCycleType);
	char tagTemplate[200];
	getTagTemplateWithOldType(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), oldCycleType, newCycleType, env->_cycleState->_verboseContextID, omrtime_current_time_millis());

	enterAtomicReportingBlock();
	writer->formatAndOutput(env, 0, "<cycle-continue %s />", tagTemplate);
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleCycleEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_GCPostCycleEndEvent* event = (MM_GCPostCycleEndEvent*)eventData;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	const char* cycleType = getCurrentCycleType(env);
	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), cycleType, env->_cycleState->_verboseContextID, omrtime_current_time_millis());

	enterAtomicReportingBlock();
	if(hasCycleEndInnerStanzas()) {
		writer->formatAndOutput(env, 0, "<cycle-end %s>", tagTemplate);
		handleCycleEndInnerStanzas(hook, eventNum, eventData, 1);
		writer->formatAndOutput(env, 0, "</cycle-end>");
	} else {
		writer->formatAndOutput(env, 0, "<cycle-end %s />", tagTemplate);
	}
	writer->flush(env);
	exitAtomicReportingBlock();
}

bool
MM_VerboseHandlerOutput::hasCycleStartInnerStanzas()
{
	return false;
}
void
MM_VerboseHandlerOutput::handleCycleStartInnerStanzas(J9HookInterface** hook, uintptr_t eventNum, void* eventData, uintptr_t indentDepth)
{
	/* do nothing */
}

bool
MM_VerboseHandlerOutput::hasCycleEndInnerStanzas()
{
	return false;
}
void
MM_VerboseHandlerOutput::handleCycleEndInnerStanzas(J9HookInterface** hook, uintptr_t eventNum, void* eventData, uintptr_t indentDepth)
{
	/* do nothing */
}

const char *
MM_VerboseHandlerOutput::getCycleType(uintptr_t type)
{
	return "unknown";
}

const char *
MM_VerboseHandlerOutput::getCurrentCycleType(MM_EnvironmentBase *env) {
	return getCycleType(env->_cycleState->_type);
}

void
MM_VerboseHandlerOutput::handleExclusiveStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ExclusiveAccessAcquireEvent* event = (MM_ExclusiveAccessAcquireEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	uint64_t exclusiveAccessTime = omrtime_hires_delta(0, event->exclusiveAccessTime, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
	uint64_t meanIdleTime = omrtime_hires_delta(0, event->meanIdleTime, OMRPORT_TIME_DELTA_IN_MICROSECONDS);

	uint64_t currentTime = event->timestamp;
	uint64_t previousTime =  manager->getLastExclusiveAccessStartTime();
	if (0 == previousTime) {
		previousTime = manager->getInitializedTime();
	}
	uint64_t deltaTime = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&deltaTime, previousTime, currentTime);

	manager->setLastExclusiveAccessStartTime(currentTime);

	OMR_VMThread* lastResponder = event->lastResponder;
	char escapedLastResponderName[64];
	getThreadName(escapedLastResponderName,sizeof(escapedLastResponderName),lastResponder);

	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), omrtime_current_time_millis());
	enterAtomicReportingBlock();
	if (!deltaTimeSuccess) {
		writer->formatAndOutput(env, 0, "<warning details=\"clock error detected, following timing may be inaccurate\" />");
	}	
	writer->formatAndOutput(env, 0, "<exclusive-start %s intervalms=\"%llu.%03.3llu\">", tagTemplate, deltaTime / 1000, deltaTime % 1000);
	writer->formatAndOutput(env, 1, "<response-info timems=\"%llu.%03.3llu\" idlems=\"%llu.%03.3llu\" threads=\"%zu\" lastid=\"%p\" lastname=\"%s\" />",
			exclusiveAccessTime / 1000, exclusiveAccessTime % 1000, meanIdleTime / 1000, meanIdleTime % 1000, event->haltedThreads, (NULL == lastResponder ? NULL : lastResponder->_language_vmthread), escapedLastResponderName);
	writer->formatAndOutput(env, 0, "</exclusive-start>");
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleExclusiveEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ExclusiveAccessReleaseEvent* event = (MM_ExclusiveAccessReleaseEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uint64_t currentTime = event->timestamp;
	uint64_t startTime = manager->getLastExclusiveAccessStartTime();
	manager->setLastExclusiveAccessEndTime(currentTime);
	uint64_t deltaTime = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&deltaTime, startTime, currentTime);


	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), omrtime_current_time_millis());
	enterAtomicReportingBlock();
	if (!deltaTimeSuccess) {
		writer->formatAndOutput(env, 0, "<warning details=\"clock error detected, following timing may be inaccurate\" />");
	}
	writer->formatAndOutput(env, 0, "<exclusive-end %s durationms=\"%llu.%03llu\" />", tagTemplate, deltaTime / 1000, deltaTime % 1000);
	writer->formatAndOutput(env, 0, "");
	writer->flush(env);
	writer->endOfCycle(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleSystemGCStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_SystemGCStartEvent* event = (MM_SystemGCStartEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uint64_t currentTime = event->timestamp;
	uint64_t previousTime =  manager->getLastSystemGCTime();
	if (0 == previousTime) {
		previousTime = manager->getInitializedTime();
	}

	uint64_t deltaTime = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&deltaTime, previousTime, currentTime);

	manager->setLastSystemGCTime(currentTime);
	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), omrtime_current_time_millis());
	enterAtomicReportingBlock();
	if (!deltaTimeSuccess) {
		writer->formatAndOutput(env, 0, "<warning details=\"clock error detected, following timing may be inaccurate\" />");
	}	
	writer->formatAndOutput(env, 0, "<sys-start reason=\"%s\" %s intervalms=\"%llu.%03llu\" />", getSystemGCReasonAsString(event->gcCode), tagTemplate, deltaTime / 1000 , deltaTime % 1000);
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleSystemGCEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_SystemGCEndEvent* event = (MM_SystemGCEndEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), omrtime_current_time_millis());
	enterAtomicReportingBlock();
	writer->formatAndOutput(env, 0, "<sys-end %s />", tagTemplate);
	writer->flush(env);
	exitAtomicReportingBlock();
}

bool
MM_VerboseHandlerOutput::hasAllocationFailureStartInnerStanzas()
{
	return false;
}

void
MM_VerboseHandlerOutput::handleAllocationFailureStartInnerStanzas(J9HookInterface** hook, uintptr_t eventNum, void* eventData, uintptr_t indentDepth)
{

}

void
MM_VerboseHandlerOutput::handleAllocationFailureStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_AllocationFailureStartEvent* event = (MM_AllocationFailureStartEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uint64_t currentTime = event->timestamp;
	uint64_t previousTime = manager->getLastAllocationFailureTime();
	manager->setLastAllocationFailureTime(currentTime);
	if (0 == previousTime) {
		previousTime = manager->getInitializedTime();
	}
	uint64_t deltaTime = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&deltaTime, previousTime, currentTime);

	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), omrtime_current_time_millis());
	enterAtomicReportingBlock();
	if (!deltaTimeSuccess) {
		writer->formatAndOutput(env, 0, "<warning details=\"clock error detected, following timing may be inaccurate\" />");
	}	

	const char *endOfTag = hasAllocationFailureStartInnerStanzas()? ">" : "/>";

	if (gc_policy_gencon == _extensions->configurationOptions._gcPolicy) {
		writer->formatAndOutput(env, 0, "<af-start id=\"%zu\" threadId=\"%p\" totalBytesRequested=\"%zu\" %s intervalms=\"%llu.%03llu\" type=\"%s\" %s", manager->getIdAndIncrement(), event->currentThread, event->requestedBytes, tagTemplate, deltaTime / 1000 , deltaTime % 1000, event->tenure? "tenure" : "nursery", endOfTag);
	} else {
		writer->formatAndOutput(env, 0, "<af-start id=\"%zu\" threadId=\"%p\" totalBytesRequested=\"%zu\" %s intervalms=\"%llu.%03llu\" %s", manager->getIdAndIncrement(), event->currentThread, event->requestedBytes, tagTemplate, deltaTime / 1000 , deltaTime % 1000, endOfTag);
	}
	if (hasAllocationFailureStartInnerStanzas()) {
		handleAllocationFailureStartInnerStanzas(hook, eventNum, eventData, 1);
		writer->formatAndOutput(env, 0, "</af-start>");
	}
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleFailedAllocationCompleted(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_FailedAllocationCompleted* event = (MM_FailedAllocationCompleted*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	OMR_VMThread *currentThread = event->currentThread;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	enterAtomicReportingBlock();
	getTagTemplate(tagTemplate, sizeof(tagTemplate), omrtime_current_time_millis());
	uintptr_t id = manager->getIdAndIncrement();
	uintptr_t bytesRequested = event->bytesRequested;
	if (TRUE == event->succeeded) {
		writer->formatAndOutput(env, 0, "<allocation-satisfied id=\"%zu\" threadId=\"%p\" bytesRequested=\"%zu\" />", id, currentThread->_language_vmthread, bytesRequested, tagTemplate);
	} else {
		writer->formatAndOutput(env, 0, "<allocation-unsatisfied id=\"%zu\" threadId=\"%p\" bytesRequested=\"%zu\" />", id, currentThread->_language_vmthread, bytesRequested, tagTemplate);
	}
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleAllocationFailureEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_AllocationFailureEndEvent* event = (MM_AllocationFailureEndEvent*)eventData;
	MM_AllocateDescription *allocDescription = event->allocDescription;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), omrtime_current_time_millis());

	const bool succeeded = allocDescription->getAllocationSucceeded();
	const char *successString = succeeded? "true" : "false";

	enterAtomicReportingBlock();

	if ((gc_policy_gencon == _extensions->configurationOptions._gcPolicy) && succeeded) {
		const char *region;
		if (allocDescription->isNurseryAllocation()) {
			region = "nursery";
		} else if (_extensions->largeObjectArea){
			region = allocDescription->isLOAAllocation()? "tenure-loa" : "tenure-soa";
		} else {
			region = "tenure";
		}
		writer->formatAndOutput(env, 0, "<af-end %s threadId=\"%p\" success=\"%s\" from=\"%s\"/>", tagTemplate, event->currentThread, successString, region);
	} else {
		writer->formatAndOutput(env, 0, "<af-end %s threadId=\"%p\" success=\"%s\" />", tagTemplate, event->currentThread, successString);
	}

	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleAcquiredExclusiveToSatisfyAllocation(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_AcquiredExclusiveToSatisfyAllocation * event = (MM_AcquiredExclusiveToSatisfyAllocation *)eventData;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	uintptr_t indentLevel = _manager->getIndentLevel();

	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), omrtime_current_time_millis());
	enterAtomicReportingBlock();
	writer->formatAndOutput(env, indentLevel, "<event %s>", tagTemplate);
	writer->formatAndOutput(env, indentLevel + 1, "<warning details=\"exclusive access acquired to satisfy allocation\" />");
	writer->formatAndOutput(env, indentLevel, "</event>");
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::enterAtomicReportingBlock()
{
	/* default implementation needs no special support */
}

void
MM_VerboseHandlerOutput::exitAtomicReportingBlock()
{
	/* default implementation needs no special support */
}

void
MM_VerboseHandlerOutput::outputMemoryInfo(MM_EnvironmentBase *env, uintptr_t indent, MM_CollectionStatistics *stats)
{
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	uintptr_t freeMemory = stats->_totalFreeHeapSize;
	uintptr_t totalMemory = stats->_totalHeapSize;

	if (hasOutputMemoryInfoInnerStanza()) {
		writer->formatAndOutput(env, indent, "<mem-info id=\"%zu\" free=\"%zu\" total=\"%zu\" percent=\"%zu\">",
				_manager->getIdAndIncrement(), freeMemory, totalMemory,
				((totalMemory == 0) ? 0 : ((uintptr_t)(((uint64_t)freeMemory*100) / (uint64_t)totalMemory))));
		outputMemoryInfoInnerStanza(env, indent + 1, stats);
		writer->formatAndOutput(env, indent, "</mem-info>");
	} else {
		writer->formatAndOutput(env, indent, "<mem-info id=\"%zu\" free=\"%zu\" total=\"%zu\" percent=\"%zu\" />",
				_manager->getIdAndIncrement(), freeMemory, totalMemory,
				((totalMemory == 0) ? 0 : ((uintptr_t)(((uint64_t)freeMemory*100) / (uint64_t)totalMemory))));
	}
	writer->flush(env);
}

bool
MM_VerboseHandlerOutput::hasOutputMemoryInfoInnerStanza()
{
	return false;
}

void
MM_VerboseHandlerOutput::outputMemoryInfoInnerStanza(MM_EnvironmentBase *env, uintptr_t indent, MM_CollectionStatistics *stats)
{
}

void
MM_VerboseHandlerOutput::printAllocationStats(MM_EnvironmentBase* env)
{
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	OMR_VMThread * vmThreadAllocatedMost = _extensions->vmThreadAllocatedMost;
	MM_AllocationStats* systemStats = &_extensions->allocationStats;
	bool consumedEntireThreadName = false;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	enterAtomicReportingBlock();
	writer->formatAndOutput(env, 0, "<allocation-stats totalBytes=\"%zu\" >", systemStats->bytesAllocated());

	if (_extensions->isVLHGC()) {
#if defined(OMR_GC_VLHGC)
		writer->formatAndOutput(env, 1, "<allocated-bytes non-tlh=\"%zu\" tlh=\"%zu\" arrayletleaf=\"%zu\"/>",
				systemStats->nontlhBytesAllocated(), systemStats->tlhBytesAllocated(), systemStats->_arrayletLeafAllocationBytes);
#endif /* OMR_GC_VLHGC */
	} else if (_extensions->isStandardGC()) {
#if defined(OMR_GC_MODRON_STANDARD)
		writer->formatAndOutput(env, 1, "<allocated-bytes non-tlh=\"%zu\" tlh=\"%zu\" />", systemStats->nontlhBytesAllocated(), systemStats->tlhBytesAllocated());
#endif /* OMR_GC_MODRON_STANDARD */
	} else {
		/* for now, not covered the case of specs that do not have TLHs, but have arraylets */
	}

	if(0 != _extensions->bytesAllocatedMost){
		const char *dots = "";
		char escapedThreadName[128];
		void *threadID = NULL;
		if (NULL != vmThreadAllocatedMost) {
			consumedEntireThreadName = getThreadName(escapedThreadName, sizeof(escapedThreadName), vmThreadAllocatedMost);
			dots = consumedEntireThreadName ? "" : "...";
			threadID = vmThreadAllocatedMost->_language_vmthread;
		} else {
			omrstr_printf(escapedThreadName, sizeof(escapedThreadName), "unknown thread");
		}
		writer->formatAndOutput(env, 1, "<largest-consumer threadName=\"%s%s\" threadId=\"%p\" bytes=\"%zu\" />", escapedThreadName, dots, threadID, _extensions->bytesAllocatedMost);
	}
	writer->formatAndOutput(env, 0, "</allocation-stats>");
	writer->flush(env);
	exitAtomicReportingBlock();
}


void
MM_VerboseHandlerOutput::handleGCStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_GCIncrementStartEvent * event = (MM_GCIncrementStartEvent *)eventData;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_CollectionStatistics *stats = (MM_CollectionStatistics *)event->stats;
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), getCurrentCycleType(env), env->_cycleState->_verboseContextID, omrtime_current_time_millis());

	enterAtomicReportingBlock();
	writer->formatAndOutput(env, 0, "<gc-start %s>", tagTemplate);
	outputMemoryInfo(env, _manager->getIndentLevel() + 1, stats);
	writer->formatAndOutput(env, 0, "</gc-start>");
	exitAtomicReportingBlock();

	printAllocationStats(env);
}

void
MM_VerboseHandlerOutput::handleGCEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_GCIncrementEndEvent * event = (MM_GCIncrementEndEvent *)eventData;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_CollectionStatistics *stats = (MM_CollectionStatistics *)event->stats;
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uint64_t durationInMicroseconds   = 0;
	uint64_t userTimeInMicroseconds   = 0;
	uint64_t systemTimeInMicroseconds = 0;

	/* convert from nanoseconds to microseconds, compiler should autocast lvalue (int64_t) to rvalue (uint64_t) but just incase */
	uint64_t startUserTime   =	(uint64_t)stats->_startProcessTimes._userTime   / 1000;
	uint64_t startSystemTime =	(uint64_t)stats->_startProcessTimes._systemTime / 1000;

	uint64_t endUserTime     =	(uint64_t)stats->_endProcessTimes._userTime	 	/ 1000;
	uint64_t endSystemTime   =	(uint64_t)stats->_endProcessTimes._systemTime   / 1000;

	bool getDurationTimeSuccessful = getTimeDeltaInMicroSeconds(&durationInMicroseconds, stats->_startTime, stats->_endTime);
	bool getUserTimeSuccessful = getTimeDelta(&userTimeInMicroseconds, startUserTime, endUserTime);
	bool getSystemTimeSuccessful = getTimeDelta(&systemTimeInMicroseconds, startSystemTime, endSystemTime);
	char tagTemplate[200];
	getTagTemplateWithDuration(	tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(),
								getCurrentCycleType(env), env->_cycleState->_verboseContextID,
								durationInMicroseconds, userTimeInMicroseconds, systemTimeInMicroseconds,
								omrtime_current_time_millis());
								
	uintptr_t activeThreads = env->getExtensions()->dispatcher->activeThreadCount();

	enterAtomicReportingBlock();
	if (!getDurationTimeSuccessful || !getUserTimeSuccessful || !getSystemTimeSuccessful) {
		writer->formatAndOutput(env, 0, "<warning details=\"clock error detected, following timing may be inaccurate\" />");
	}
	writer->formatAndOutput(env, 0, "<gc-end %s activeThreads=\"%zu\">", tagTemplate, activeThreads);
	outputMemoryInfo(env, _manager->getIndentLevel() + 1, stats);
	writer->formatAndOutput(env, 0, "</gc-end>");
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleHeapResize(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_HeapResizeEvent * event = (MM_HeapResizeEvent *)eventData;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	HeapResizeType resizeType = (HeapResizeType) event->resizeType;
	uintptr_t resizeAmount = event->amount;
	uintptr_t subSpaceType = event->subSpaceType;
	uint64_t timeInMicroSeconds = event->timeTaken;
	uintptr_t reason = event->reason;
	uintptr_t resizeCount = 1;

	if ((0 == resizeAmount) || ((HEAP_EXPAND == resizeType) && (SATISFY_COLLECTOR == (ExpandReason)reason))) {
		/* The SATISFY_COLLECTOR output will be handled by the copy forward collectors directly */
		return;
	}
	
	enterAtomicReportingBlock();
	outputHeapResizeInfo(env, _manager->getIndentLevel(), resizeType, resizeAmount, resizeCount, subSpaceType, reason, timeInMicroSeconds);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::outputHeapResizeInfo(MM_EnvironmentBase *env, uintptr_t indent, HeapResizeType resizeType, uintptr_t resizeAmount, uintptr_t resizeCount, uintptr_t subSpaceType, uintptr_t reason, uint64_t timeInMicroSeconds)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	uintptr_t id = _manager->getIdAndIncrement();
	const char *reasonString = NULL;
	const char *resizeTypeName = NULL;
	char tagTemplate[200];

	if (HEAP_EXPAND == resizeType) {
		resizeTypeName = "expand";
		reasonString = getExpandReasonAsString((ExpandReason)reason);
	} else if (HEAP_CONTRACT == resizeType) {
		resizeTypeName = "contract";
		reasonString = getContractReasonAsString((ContractReason)reason);
	} else if (HEAP_LOA_EXPAND == resizeType) {
		resizeTypeName = "loa expand";
		reasonString = getLoaResizeReasonAsString((LoaResizeReason)reason);
	} else if (HEAP_LOA_CONTRACT == resizeType) {
		resizeTypeName = "loa contract";
		reasonString = getLoaResizeReasonAsString((LoaResizeReason)reason);
	} else {
		resizeTypeName = "unknown";
		reasonString = "unknown";
	}

	getTagTemplate(tagTemplate, sizeof(tagTemplate), omrtime_current_time_millis());

	writer->formatAndOutput(env, indent, "<heap-resize id=\"%zu\" type=\"%s\" space=\"%s\" amount=\"%zu\" count=\"%zu\" timems=\"%llu.%03llu\" reason=\"%s\" %s />", id, resizeTypeName, getSubSpaceType(subSpaceType), resizeAmount, resizeCount, timeInMicroSeconds / 1000, timeInMicroSeconds % 1000, reasonString, tagTemplate);
}

void
MM_VerboseHandlerOutput::outputCollectorHeapResizeInfo(MM_EnvironmentBase *env, uintptr_t indent, HeapResizeType resizeType, uintptr_t resizeAmount, uintptr_t resizeCount, uintptr_t subSpaceType, uintptr_t reason, uint64_t timeInMicroSeconds)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	const char *reasonString = NULL;
	const char *resizeTypeName = NULL;
	char tagTemplate[200];

	if (HEAP_EXPAND == resizeType) {
		resizeTypeName = "expand";
		reasonString = getExpandReasonAsString((ExpandReason)reason);
	} else if (HEAP_CONTRACT == resizeType) {
		resizeTypeName = "contract";
		reasonString = getContractReasonAsString((ContractReason)reason);
	} else {
		resizeTypeName = "unknown";
		reasonString = "unknown";
	}

	getTagTemplate(tagTemplate, sizeof(tagTemplate), omrtime_current_time_millis());

	writer->formatAndOutput(env, indent, "<heap-resize type=\"%s\" space=\"%s\" amount=\"%zu\" count=\"%zu\" timems=\"%llu.%03llu\" reason=\"%s\" />", resizeTypeName, getSubSpaceType(subSpaceType), resizeAmount, resizeCount, timeInMicroSeconds / 1000, timeInMicroSeconds % 1000, reasonString);
}

const char *
MM_VerboseHandlerOutput::getSubSpaceType(uintptr_t typeFlags)
{
	return "default";
}

void
MM_VerboseHandlerOutput::handleExcessiveGCRaised(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ExcessiveGCRaisedEvent * event = (MM_ExcessiveGCRaisedEvent *)eventData;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	uintptr_t indentLevel = _manager->getIndentLevel();

	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), omrtime_current_time_millis());
	enterAtomicReportingBlock();
	writer->formatAndOutput(env, indentLevel, "<event %s>", tagTemplate);

	switch(event->excessiveLevel) {
	case excessive_gc_aggressive:
		writer->formatAndOutput(env, indentLevel + 1, "<warning details=\"excessive gc activity detected, will attempt aggressive gc\" />");
		break;
	case excessive_gc_fatal:
	case excessive_gc_fatal_consumed:
		writer->formatAndOutput(env, indentLevel + 1, "<warning details=\"excessive gc activity detected, will fail on allocate\" />");
		break;
	default:
		writer->formatAndOutput(env, indentLevel + 1, "<warning details=\"excessive gc activity detected, unknown level: %d \" />", event->excessiveLevel);
		break;
	}

	writer->formatAndOutput(env, 0, "</event>");
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::outputStringConstantInfo(MM_EnvironmentBase *env, uintptr_t ident, uintptr_t candidates, uintptr_t cleared)
{
	MM_VerboseWriterChain* writer = _manager->getWriterChain();

	if (0 != candidates) {
		writer->formatAndOutput(env, ident, "<stringconstants candidates=\"%zu\" cleared=\"%zu\"  />", candidates, cleared);
	}
}

void
verboseHandlerInitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput*)userData)->handleInitialized(hook, eventNum, eventData);
}

void
verboseHandlerHeapResize(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput*)userData)->handleHeapResize(hook, eventNum, eventData);
}
