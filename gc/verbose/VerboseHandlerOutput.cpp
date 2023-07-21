/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "AllocateDescription.hpp"
#include "AllocationStats.hpp"
#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "CollectionStatistics.hpp"
#include "ConcurrentPhaseStatsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionManager.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ParallelDispatcher.hpp"
#include "VerboseHandlerOutput.hpp"
#include "VerboseManager.hpp"
#include "VerboseWriterChain.hpp"
#include "VerboseBuffer.hpp"

#include "gcutils.h"

static void verboseHandlerInitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
#if defined(J9VM_OPT_CRIU_SUPPORT)
static void verboseHandlerReinitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
static void verboseHandlerHeapResize(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);

MM_VerboseHandlerOutput *
MM_VerboseHandlerOutput::newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager)
{
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());

	MM_VerboseHandlerOutput *verboseHandlerOutput = (MM_VerboseHandlerOutput*)extensions->getForge()->allocate(sizeof(MM_VerboseHandlerOutput), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
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
	_reportingLock()
	,_extensions(extensions)
	,_omrVM(NULL)
	,_mmPrivateHooks(NULL)
	,_mmOmrHooks(NULL)
	,_manager(NULL)
{}

bool
MM_VerboseHandlerOutput::initialize(MM_EnvironmentBase *env, MM_VerboseManager *manager)
{
	_omrVM = env->getOmrVM();
	_mmPrivateHooks = J9_HOOK_INTERFACE(_extensions->privateHookInterface);
	_mmOmrHooks = J9_HOOK_INTERFACE(_extensions->omrHookInterface);
	_manager = manager;

	if (!_reportingLock.initialize(env, &env->getExtensions()->lnrlOptions, "MM_VerboseHandlerOutput:_reportingLock")) {
		return false;
	}

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
	_reportingLock.tearDown();
	return ;
}

void
MM_VerboseHandlerOutput::enableVerbose()
{
	/* Initialized */
	(*_mmOmrHooks)->J9HookRegisterWithCallSite(_mmOmrHooks, J9HOOK_MM_OMR_INITIALIZED, verboseHandlerInitialized, OMR_GET_CALLSITE(), (void *)this);
#if defined(J9VM_OPT_CRIU_SUPPORT)
	(*_mmOmrHooks)->J9HookRegisterWithCallSite(_mmOmrHooks, J9HOOK_MM_OMR_REINITIALIZED, verboseHandlerReinitialized, OMR_GET_CALLSITE(), (void *)this);
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	(*_mmPrivateHooks)->J9HookRegisterWithCallSite(_mmPrivateHooks, J9HOOK_MM_PRIVATE_HEAP_RESIZE, verboseHandlerHeapResize, OMR_GET_CALLSITE(), (void *)this);

	return ;
}

void
MM_VerboseHandlerOutput::disableVerbose()
{
	/* Initialized */
	(*_mmOmrHooks)->J9HookUnregister(_mmOmrHooks, J9HOOK_MM_OMR_INITIALIZED, verboseHandlerInitialized, NULL);
#if defined(J9VM_OPT_CRIU_SUPPORT)
	(*_mmOmrHooks)->J9HookUnregister(_mmOmrHooks, J9HOOK_MM_OMR_REINITIALIZED, verboseHandlerReinitialized, NULL);
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_HEAP_RESIZE, verboseHandlerHeapResize, NULL);

	return ;
}

bool
MM_VerboseHandlerOutput::getThreadName(char *buf, uintptr_t bufLen, OMR_VMThread *vmThread)
{
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	omrstr_printf(buf, bufLen, "OMR_VMThread [%p]",vmThread);

	return true;
}

void
MM_VerboseHandlerOutput::writeVmArgs(MM_EnvironmentBase* env, MM_VerboseBuffer* buffer)
{
	/* TODO (stefanbu) OMR does not support argument parsing yet, but we should repsect schema.*/
	buffer->formatAndOutput(env, 1, "<vmargs>");
	buffer->formatAndOutput(env, 1, "</vmargs>");
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
MM_VerboseHandlerOutput::getTagTemplate(char *buf, uintptr_t bufsize, uintptr_t id, uintptr_t contextId, uint64_t wallTimeMs)
{
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	uintptr_t bufPos = 0;
	bufPos += getTagTemplate(buf, bufsize, id, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, " contextid=\"%zu\"", contextId);

	return bufPos;
}

uintptr_t
MM_VerboseHandlerOutput::getTagTemplate(char *buf, uintptr_t bufsize, uintptr_t id, const char *type, uintptr_t contextId, uint64_t wallTimeMs, const char *reasonForTermination)
{
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	uintptr_t bufPos = 0;
	bufPos += omrstr_printf(buf, bufsize, "id=\"%zu\" type=\"%s\" contextid=\"%zu\" timestamp=\"", id, type, contextId);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_PRE_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "%03llu", wallTimeMs % 1000);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_POST_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "\"");

	if (NULL != reasonForTermination) {
		bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, " terminationReason=\"%s\"", reasonForTermination);
	}

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
MM_VerboseHandlerOutput::getTagTemplateWithDuration(char *buf, uintptr_t bufsize, uintptr_t id, const char *type, uintptr_t contextId, uint64_t durationus, uint64_t usertimeus, uint64_t cputimeus, uint64_t wallTimeMs, uint64_t stalltimeus)
{
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	uintptr_t bufPos = 0;
	bufPos += omrstr_printf(buf, bufsize, "id=\"%zu\" type=\"%s\" contextid=\"%zu\" durationms=\"%llu.%03.3llu\" usertimems=\"%llu.%03.3llu\" systemtimems=\"%llu.%03.3llu\" stalltimems=\"%llu.%03.3llu\" timestamp=\"",
 			id, type, contextId, durationus / 1000, durationus % 1000, usertimeus / 1000, usertimeus % 1000, cputimeus / 1000, cputimeus % 1000, stalltimeus / 1000, stalltimeus % 1000);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_PRE_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "%03llu", wallTimeMs % 1000);
	bufPos += omrstr_ftime(buf + bufPos, bufsize - bufPos, VERBOSEGC_DATE_FORMAT_POST_MS, wallTimeMs);
	bufPos += omrstr_printf(buf + bufPos, bufsize - bufPos, "\"");

	return bufPos;
}

const char *
MM_VerboseHandlerOutput::getConcurrentTerminationReason(MM_ConcurrentPhaseStatsBase *stats)
{
	const char *reasonForTermination = NULL;
	if (stats->isTerminationRequested()) {
		if (stats->isTerminationRequestExternal()) {
			/* For example, Java JVMTI and similar. Unfortunately, we could not tell what. */
			reasonForTermination = "termination requested externally";
		} else {
			/* Most interesting reason would be exhausted allocate/survivor, since it could mean that
			 * tilting is too aggressive (and survival rate is jittery), and suggest tilt tuning/limiting.
			 * There could be various other reasons, like STW global GC (system, end of concurrent mark etc.),
			 * or even notorious 'exclusive VM access to satisfy allocate'.
			 * Either way, the more detailed reason could be deduced from verbose GC.
			 */
			reasonForTermination = "termination requested by GC";
		}
	}
	return reasonForTermination;
}

/**
 * Return the reason for heap fixup as a string
 * @param reason reason code
 */
const char *
MM_VerboseHandlerOutput::getHeapFixupReasonString(uintptr_t reason)
{
	switch((FixUpReason)reason) {
		case FIXUP_NONE:
			return "no fixup";
		case FIXUP_CLASS_UNLOADING:
			return "class unloading";
		case FIXUP_DEBUG_TOOLING:
			return "debug tooling";
		default:
			return "unknown";
	}
}

void
MM_VerboseHandlerOutput::outputInitializedStanza(MM_EnvironmentBase *env, MM_VerboseBuffer *buffer)
{
	char tagTemplate[200];

	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	Assert_MM_true(_manager->getInitializedTime() != 0);
	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), omrtime_current_time_millis());

	buffer->formatAndOutput(env, 0, "<initialized %s>", tagTemplate);
	buffer->formatAndOutput(env, 1, "<attribute name=\"gcPolicy\" value=\"%s\" />", _extensions->gcModeString);

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	if (_extensions->isConcurrentScavengerEnabled()) {
		buffer->formatAndOutput(env, 1, "<attribute name=\"concurrentScavenger\" value=\"%s\" />",
#if defined(S390) || defined(J9ZOS390)
				_extensions->concurrentScavengerHWSupport ?
				"enabled, with H/W assistance" :
				"enabled, without H/W assistance");
#else /* defined(S390) || defined(J9ZOS390) */
				"enabled");
#endif /* defined(S390) || defined(J9ZOS390) */
	}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

	buffer->formatAndOutput(env, 1, "<attribute name=\"maxHeapSize\" value=\"0x%zx\" />", _extensions->memoryMax);
	buffer->formatAndOutput(env, 1, "<attribute name=\"initialHeapSize\" value=\"0x%zx\" />", _extensions->initialMemorySize);

#if defined(OMR_GC_COMPRESSED_POINTERS)
	if (env->compressObjectReferences()) {
		buffer->formatAndOutput(env, 1, "<attribute name=\"compressedRefs\" value=\"true\" />");
		buffer->formatAndOutput(env, 1, "<attribute name=\"compressedRefsDisplacement\" value=\"0x%zx\" />", 0);
		buffer->formatAndOutput(env, 1, "<attribute name=\"compressedRefsShift\" value=\"0x%zx\" />", _omrVM->_compressedPointersShift);
	} else
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	{
		buffer->formatAndOutput(env, 1, "<attribute name=\"compressedRefs\" value=\"false\" />");
	}

	buffer->formatAndOutput(env, 1, "<attribute name=\"pageSize\" value=\"0x%zx\" />", _extensions->heap->getPageSize());
	buffer->formatAndOutput(env, 1, "<attribute name=\"pageType\" value=\"%s\" />", getPageTypeString(_extensions->heap->getPageFlags()));
	buffer->formatAndOutput(env, 1, "<attribute name=\"requestedPageSize\" value=\"0x%zx\" />", _extensions->requestedPageSize);
	buffer->formatAndOutput(env, 1, "<attribute name=\"requestedPageType\" value=\"%s\" />", getPageTypeString(_extensions->requestedPageFlags));
	buffer->formatAndOutput(env, 1, "<attribute name=\"gcthreads\" value=\"%zu\" />", _extensions->gcThreadCount);

	if (gc_policy_gencon == _extensions->configurationOptions._gcPolicy) {
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		if (_extensions->isConcurrentScavengerEnabled()) {
			buffer->formatAndOutput(env, 1, "<attribute name=\"gcthreads Concurrent Scavenger\" value=\"%zu\" />", _extensions->concurrentScavengerBackgroundThreads);
		}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
		if (_extensions->isConcurrentMarkEnabled()) {
			buffer->formatAndOutput(env, 1, "<attribute name=\"gcthreads Concurrent Mark\" value=\"%zu\" />", _extensions->concurrentBackground);
		}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
	}

	buffer->formatAndOutput(env, 1, "<attribute name=\"packetListSplit\" value=\"%zu\" />", _extensions->packetListSplit);
#if defined(OMR_GC_MODRON_SCAVENGER)
	buffer->formatAndOutput(env, 1, "<attribute name=\"cacheListSplit\" value=\"%zu\" />", _extensions->cacheListSplit);
#endif /* OMR_GC_MODRON_SCAVENGER */
	buffer->formatAndOutput(env, 1, "<attribute name=\"splitFreeListSplitAmount\" value=\"%zu\" />", _extensions->splitFreeListSplitAmount);
	buffer->formatAndOutput(env, 1, "<attribute name=\"numaNodes\" value=\"%zu\" />", _extensions->_numaManager.getAffinityLeaderCount());
#if defined(J9VM_OPT_CRIU_SUPPORT)
	if (_extensions->reinitializationInProgress()) {
		buffer->formatAndOutput(env, 1, "<attribute name=\"Restored Snapshot\" value=\"%s\" />", "true");
	}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

	outputInitializedInnerStanza(env, buffer);

	buffer->formatAndOutput(env, 1, "<system>");
	buffer->formatAndOutput(env, 2, "<attribute name=\"physicalMemory\" value=\"%llu\" />", omrsysinfo_get_physical_memory());
	buffer->formatAndOutput(env, 2, "<attribute name=\"numCPUs\" value=\"%zu\" />", omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE));
	buffer->formatAndOutput(env, 2, "<attribute name=\"architecture\" value=\"%s\" />", omrsysinfo_get_CPU_architecture());
	buffer->formatAndOutput(env, 2, "<attribute name=\"os\" value=\"%s\" />", omrsysinfo_get_OS_type());
	buffer->formatAndOutput(env, 2, "<attribute name=\"osVersion\" value=\"%s\" />", omrsysinfo_get_OS_version());
	buffer->formatAndOutput(env, 1, "</system>");

	writeVmArgs(env,buffer);

	buffer->formatAndOutput(env, 0, "</initialized>\n");
}

void
MM_VerboseHandlerOutput::outputInitializedRegion(MM_EnvironmentBase *env, MM_VerboseBuffer *buffer)
{
	OMR_VM *omrVM = env->getOmrVM();
#if defined(OMR_GC_DOUBLE_MAP_ARRAYLETS)
	bool isArrayletDoubleMapRequested = _extensions->isArrayletDoubleMapRequested;
	const char *arrayletDoubleMappingStatus = _extensions->indexableObjectModel.isDoubleMappingEnabled() ? "enabled" : "disabled";
	const char *arrayletDoubleMappingRequested = isArrayletDoubleMapRequested ? "true" : "false";
#endif /* OMR_GC_DOUBLE_MAP_ARRAYLETS */
#if defined(OMR_GC_SPARSE_HEAP_ALLOCATION)
	bool isVirtualLargeObjectHeapRequested = _extensions->isVirtualLargeObjectHeapRequested;
	const char *virtualLargeObjectHeapStatus = _extensions->isVirtualLargeObjectHeapEnabled ? "enabled" : "disabled";
	const char *virtualLargeObjectHeapRequested = isVirtualLargeObjectHeapRequested ? "true" : "false";
#endif /* OMR_GC_SPARSE_HEAP_ALLOCATION */
	buffer->formatAndOutput(env, 1, "<region>");
	buffer->formatAndOutput(env, 2, "<attribute name=\"regionSize\" value=\"%zu\" />", _extensions->getHeap()->getHeapRegionManager()->getRegionSize());
	buffer->formatAndOutput(env, 2, "<attribute name=\"regionCount\" value=\"%zu\" />", _extensions->getHeap()->getHeapRegionManager()->getTableRegionCount());
	buffer->formatAndOutput(env, 2, "<attribute name=\"arrayletLeafSize\" value=\"%zu\" />", omrVM->_arrayletLeafSize);
	if (_extensions->isVLHGC()) {
#if defined(OMR_GC_DOUBLE_MAP_ARRAYLETS)
		buffer->formatAndOutput(env, 2, "<attribute name=\"arrayletDoubleMappingRequested\" value=\"%s\"/>", arrayletDoubleMappingRequested);
		buffer->formatAndOutput(env, 2, "<attribute name=\"arrayletDoubleMapping\" value=\"%s\"/>", arrayletDoubleMappingStatus);
#endif /* OMR_GC_DOUBLE_MAP_ARRAYLETS */
#if defined(OMR_GC_SPARSE_HEAP_ALLOCATION)
		buffer->formatAndOutput(env, 2, "<attribute name=\"virtualLargeObjectHeapRequested\" value=\"%s\"/>", virtualLargeObjectHeapRequested);
		buffer->formatAndOutput(env, 2, "<attribute name=\"virtualLargeObjectHeapStatus\" value=\"%s\"/>", virtualLargeObjectHeapStatus);
#endif /* OMR_GC_SPARSE_HEAP_ALLOCATION */
	}
	buffer->formatAndOutput(env, 1, "</region>");
}

void
MM_VerboseHandlerOutput::handleInitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_InitializedEvent* event = (MM_InitializedEvent*)eventData;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);

 	_manager->setInitializedTime(event->timestamp);

	enterAtomicReportingBlock();
	outputInitializedStanza(env, writer->getBuffer());
	writer->flush(env);
	exitAtomicReportingBlock();
}

#if defined(J9VM_OPT_CRIU_SUPPORT)
void
MM_VerboseHandlerOutput::handleReinitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ReinitializedEvent* event = (MM_ReinitializedEvent*)eventData;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	_manager->setInitializedTime(event->timestamp);

	enterAtomicReportingBlock();
	outputInitializedStanza(env, writer->getBuffer());
	writer->flush(env);
	exitAtomicReportingBlock();
}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

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
	char fixupTagTemplate[100];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), cycleType, env->_cycleState->_verboseContextID, omrtime_current_time_millis());

	enterAtomicReportingBlock();
	if(hasCycleEndInnerStanzas()) {
		writer->formatAndOutput(env, 0, "<cycle-end %s>", tagTemplate);
		handleCycleEndInnerStanzas(hook, eventNum, eventData, 1);
		writer->formatAndOutput(env, 0, "</cycle-end>");
	} else {
		writer->formatAndOutput(env, 0, "<cycle-end %s />", tagTemplate);
	}

	if ((OMR_GC_CYCLE_TYPE_GLOBAL == event->cycleType) && (FIXUP_NONE != event->fixHeapForWalkReason)) {
		uint64_t fixupDuration = event->fixHeapForWalkTime;
		getTagTemplate(fixupTagTemplate, sizeof(fixupTagTemplate), omrtime_current_time_millis());
		writer->formatAndOutput(env, 0, "<heap-fixup timems=\"%llu.%03llu\" reason=\"%s\"  %s />", fixupDuration / 1000, fixupDuration % 1000, getHeapFixupReasonString(event->fixHeapForWalkReason), fixupTagTemplate);
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
	_reportingLock.acquire();
}

void
MM_VerboseHandlerOutput::exitAtomicReportingBlock()
{
	_reportingLock.release();
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
	uint64_t stallTimeInMicroseconds  = 0;

	/* convert from nanoseconds to microseconds, compiler should autocast lvalue (int64_t) to rvalue (uint64_t) but just incase */
	uint64_t startUserTime   =	(uint64_t)stats->_startProcessTimes._userTime   / 1000;
	uint64_t startSystemTime =	(uint64_t)stats->_startProcessTimes._systemTime / 1000;

	uint64_t endUserTime     =	(uint64_t)stats->_endProcessTimes._userTime	 	/ 1000;
	uint64_t endSystemTime   =	(uint64_t)stats->_endProcessTimes._systemTime   / 1000;

	bool getDurationTimeSuccessful = getTimeDeltaInMicroSeconds(&durationInMicroseconds, stats->_startTime, stats->_endTime);
	bool getUserTimeSuccessful = getTimeDelta(&userTimeInMicroseconds, startUserTime, endUserTime);
	bool getSystemTimeSuccessful = getTimeDelta(&systemTimeInMicroseconds, startSystemTime, endSystemTime);
	bool getStallTimeSuccessful = getTimeDeltaInMicroSeconds(&stallTimeInMicroseconds, 0, stats->_stallTime);

	char tagTemplate[200];
	getTagTemplateWithDuration(	tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(),
								getCurrentCycleType(env), env->_cycleState->_verboseContextID,
								durationInMicroseconds, userTimeInMicroseconds, systemTimeInMicroseconds,
								omrtime_current_time_millis(), stallTimeInMicroseconds);
								
	uintptr_t activeThreads = env->getExtensions()->dispatcher->activeThreadCount();

	enterAtomicReportingBlock();
	if (!getDurationTimeSuccessful || !getUserTimeSuccessful || !getSystemTimeSuccessful || !getStallTimeSuccessful) {
		writer->formatAndOutput(env, 0, "<warning details=\"clock error detected, following timing may be inaccurate\" />");
	}
	writer->formatAndOutput(env, 0, "<gc-end %s activeThreads=\"%zu\">", tagTemplate, activeThreads);
	outputMemoryInfo(env, _manager->getIndentLevel() + 1, stats);
	writer->formatAndOutput(env, 0, "</gc-end>");
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleConcurrentStart(J9HookInterface** hook, UDATA eventNum, void* eventData)
{
	MM_ConcurrentPhaseStartEvent *event = (MM_ConcurrentPhaseStartEvent *)eventData;
	MM_ConcurrentPhaseStatsBase *stats = (MM_ConcurrentPhaseStatsBase *)event->concurrentStats;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	UDATA contextId = stats->_cycleID;
	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), getConcurrentTypeString(stats->_concurrentCycleType), contextId, omrtime_current_time_millis());

	enterAtomicReportingBlock();
	writer->formatAndOutput(env, 0, "<concurrent-start %s>", tagTemplate);
	handleConcurrentStartInternal(hook, eventNum, eventData);
	writer->formatAndOutput(env, 0, "</concurrent-start>");
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutput::handleConcurrentEnd(J9HookInterface** hook, UDATA eventNum, void* eventData)
{
	MM_ConcurrentPhaseEndEvent *event = (MM_ConcurrentPhaseEndEvent *)eventData;
	MM_ConcurrentPhaseStatsBase *stats = (MM_ConcurrentPhaseStatsBase *)event->concurrentStats;
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	UDATA contextId = stats->_cycleID;
	char tagTemplate[200];
	const char* reasonForTermination = getConcurrentTerminationReason(stats);
	getTagTemplate(tagTemplate, sizeof(tagTemplate), _manager->getIdAndIncrement(), getConcurrentTypeString(stats->_concurrentCycleType), contextId, omrtime_current_time_millis(), reasonForTermination);

	enterAtomicReportingBlock();
	writer->formatAndOutput(env, 0, "<concurrent-end %s>", tagTemplate);
	handleConcurrentEndInternal(hook, eventNum, eventData);
	writer->formatAndOutput(env, 0, "</concurrent-end>\n");
	writer->flush(env);
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
	} else if (HEAP_RELEASE_FREE_PAGES == resizeType) {
		resizeTypeName = "release free pages";
		reasonString = "idle";
	} else {
		resizeTypeName = "unknown";
		reasonString = "unknown";
	}

	getTagTemplate(tagTemplate, sizeof(tagTemplate), omrtime_current_time_millis());

	writer->formatAndOutput(env, indent, "<heap-resize id=\"%zu\" type=\"%s\" space=\"%s\" amount=\"%zu\" count=\"%zu\" timems=\"%llu.%03llu\" reason=\"%s\" %s />", id, resizeTypeName, getSubSpaceType(subSpaceType), resizeAmount, resizeCount, timeInMicroSeconds / 1000, timeInMicroSeconds % 1000, reasonString, tagTemplate);
	writer->flush(env);
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
MM_VerboseHandlerOutput::outputStringConstantInfo(MM_EnvironmentBase *env, uintptr_t indent, uintptr_t candidates, uintptr_t cleared)
{
	MM_VerboseWriterChain* writer = _manager->getWriterChain();

	if (0 != candidates) {
		writer->formatAndOutput(env, indent, "<stringconstants candidates=\"%zu\" cleared=\"%zu\"  />", candidates, cleared);
	}
}

void
MM_VerboseHandlerOutput::outputMonitorReferenceInfo(MM_EnvironmentBase *env, uintptr_t indent, uintptr_t candidates, uintptr_t cleared)
{
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	if (0 != candidates) {
		writer->formatAndOutput(env, indent, "<object-monitors candidates=\"%zu\" cleared=\"%zu\"  />", candidates, cleared);
	}
}

void
verboseHandlerInitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput*)userData)->handleInitialized(hook, eventNum, eventData);
}

#if defined(J9VM_OPT_CRIU_SUPPORT)
void
verboseHandlerReinitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput*)userData)->handleReinitialized(hook, eventNum, eventData);
}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

void
verboseHandlerHeapResize(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput*)userData)->handleHeapResize(hook, eventNum, eventData);
}

void
MM_VerboseHandlerOutput::handleGCOPOuterStanzaStart(MM_EnvironmentBase* env, const char *type, uintptr_t contextID, uint64_t duration, bool deltaTimeSuccess)
{
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	if (!deltaTimeSuccess) {
		writer->formatAndOutput(env, 0, "<warning details=\"clock error detected, following timing may be inaccurate\" />");
	}

	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), type ,contextID, duration, omrtime_current_time_millis());
	writer->formatAndOutput(env, 0, "<gc-op %s>", tagTemplate);
}

void
MM_VerboseHandlerOutput::handleGCOPOuterStanzaEnd(MM_EnvironmentBase* env)
{
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	writer->formatAndOutput(env, 0, "</gc-op>");
}
