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

#include "omrgcconsts.h"
#include "gcutils.h"

#include "ConcurrentGCStats.hpp"
#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "VerboseHandlerOutputStandard.hpp"
#include "VerboseManager.hpp"
#include "VerboseWriterChain.hpp"
#include "VerboseBuffer.hpp"

static void verboseHandlerGCStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerGCEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerCycleStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerCycleEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerExclusiveStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerExclusiveEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerSystemGCStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerSystemGCEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerAllocationFailureStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerFailedAllocationCompleted(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerAllocationFailureEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerAcquiredExclusiveToSatisfyAllocation(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerMarkEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerSweepEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerExcessiveGCRaised(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);

#if defined(OMR_GC_MODRON_COMPACTION)
static void verboseHandlerCompactStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerCompactEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
#endif /* defined(OMR_GC_MODRON_COMPACTION) */

#if defined(OMR_GC_MODRON_SCAVENGER)
static void verboseHandlerScavengeEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerScavengePercolate(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
static void verboseHandlerConcurrentRememberedSetScanEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerConcurrentCardCleaningEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerConcurrentTracingEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerConcurrentKickoff(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerConcurrentHalted(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerConcurrentCollectionStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerConcurrentCollectionEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void verboseHandlerConcurrentAborted(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

MM_VerboseHandlerOutput *
MM_VerboseHandlerOutputStandard::newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager)
{
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());

	MM_VerboseHandlerOutputStandard *verboseHandlerOutput = (MM_VerboseHandlerOutputStandard *)extensions->getForge()->allocate(sizeof(MM_VerboseHandlerOutputStandard), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != verboseHandlerOutput) {
		new(verboseHandlerOutput) MM_VerboseHandlerOutputStandard(extensions);
		if(!verboseHandlerOutput->initialize(env, manager)) {
			verboseHandlerOutput->kill(env);
			verboseHandlerOutput = NULL;
		}
	}
	return verboseHandlerOutput;
}

bool
MM_VerboseHandlerOutputStandard::initialize(MM_EnvironmentBase *env, MM_VerboseManager *manager)
{
	bool initSuccess = MM_VerboseHandlerOutput::initialize(env, manager);
	return initSuccess;
}

void
MM_VerboseHandlerOutputStandard::tearDown(MM_EnvironmentBase *env)
{
	MM_VerboseHandlerOutput::tearDown(env);
}

void
MM_VerboseHandlerOutputStandard::enableVerbose()
{
	MM_VerboseHandlerOutput::enableVerbose();

	/* GCLaunch */
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SYSTEM_GC_START, verboseHandlerSystemGCStart, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SYSTEM_GC_END, verboseHandlerSystemGCEnd, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_START, verboseHandlerAllocationFailureStart, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_FAILED_ALLOCATION_COMPLETED, verboseHandlerFailedAllocationCompleted, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_END, verboseHandlerAllocationFailureEnd, (void *)this);

	/* Exclusive */
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_ACQUIRE, verboseHandlerExclusiveStart, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_RELEASE, verboseHandlerExclusiveEnd, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_ACQUIRED_EXCLUSIVE_TO_SATISFY_ALLOCATION, verboseHandlerAcquiredExclusiveToSatisfyAllocation, (void *)this);

	/* Cycle */
	(*_mmOmrHooks)->J9HookRegister(_mmOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_START, verboseHandlerCycleStart, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END, verboseHandlerCycleEnd, (void *)this);

	/* STW GC increment */
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GC_INCREMENT_START, verboseHandlerGCStart, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GC_INCREMENT_END, verboseHandlerGCEnd, (void *)this);

	/* GCOps */
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_MARK_END, verboseHandlerMarkEnd, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, verboseHandlerSweepEnd, (void *)this);
#if defined(OMR_GC_MODRON_COMPACTION)

	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_COMPACT_START, verboseHandlerCompactStart, (void *)this);

	(*_mmOmrHooks)->J9HookRegister(_mmOmrHooks, J9HOOK_MM_OMR_COMPACT_END, verboseHandlerCompactEnd, (void *)this);
#endif /* defined(OMR_GC_MODRON_COMPACTION) */
#if defined(OMR_GC_MODRON_SCAVENGER)
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SCAVENGE_END, verboseHandlerScavengeEnd, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_PERCOLATE_COLLECT, verboseHandlerScavengePercolate, (void *)this);
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	/* Concurrent */
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_KICKOFF, verboseHandlerConcurrentKickoff, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_HALTED, verboseHandlerConcurrentHalted, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_START, verboseHandlerConcurrentCollectionStart, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_END, verboseHandlerConcurrentCollectionEnd, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_ABORTED, verboseHandlerConcurrentAborted, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_REMEMBERED_SET_SCAN_END, verboseHandlerConcurrentRememberedSetScanEnd, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COMPLETE_TRACING_END, verboseHandlerConcurrentTracingEnd, (void *)this);
	(*_mmPrivateHooks)->J9HookRegister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_CARD_CLEANING_END, verboseHandlerConcurrentCardCleaningEnd, (void *)this);
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

	/* Excessive GC */
	(*_mmOmrHooks)->J9HookRegister(_mmOmrHooks, J9HOOK_MM_OMR_EXCESSIVEGC_RAISED, verboseHandlerExcessiveGCRaised, this);
}

void
MM_VerboseHandlerOutputStandard::disableVerbose()
{
	MM_VerboseHandlerOutput::disableVerbose();

	/* GCLaunch */
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SYSTEM_GC_START, verboseHandlerSystemGCStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SYSTEM_GC_END, verboseHandlerSystemGCEnd, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_START, verboseHandlerAllocationFailureStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_FAILED_ALLOCATION_COMPLETED, verboseHandlerFailedAllocationCompleted, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_END, verboseHandlerAllocationFailureEnd, NULL);

	/* Exclusive */
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_ACQUIRE, verboseHandlerExclusiveStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_RELEASE, verboseHandlerExclusiveEnd, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_ACQUIRED_EXCLUSIVE_TO_SATISFY_ALLOCATION, verboseHandlerAcquiredExclusiveToSatisfyAllocation, NULL);

	/* Cycle */
	(*_mmOmrHooks)->J9HookUnregister(_mmOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_START, verboseHandlerCycleStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END, verboseHandlerCycleEnd, NULL);

	/* STW GC increment */
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GC_INCREMENT_START, verboseHandlerGCStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_GC_INCREMENT_END, verboseHandlerGCEnd, NULL);

	/* GCOps */
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_MARK_END, verboseHandlerMarkEnd, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, verboseHandlerSweepEnd, NULL);
#if defined(OMR_GC_MODRON_COMPACTION)

	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_COMPACT_START, verboseHandlerCompactStart, NULL);

	(*_mmOmrHooks)->J9HookUnregister(_mmOmrHooks, J9HOOK_MM_OMR_COMPACT_END, verboseHandlerCompactEnd, NULL);
#endif /* defined(OMR_GC_MODRON_COMPACTION) */
#if defined(OMR_GC_MODRON_SCAVENGER)
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_SCAVENGE_END, verboseHandlerScavengeEnd, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_PERCOLATE_COLLECT, verboseHandlerScavengePercolate, NULL);
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	/* Concurrent */
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_KICKOFF, verboseHandlerConcurrentKickoff, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_HALTED, verboseHandlerConcurrentHalted, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_START, verboseHandlerConcurrentCollectionStart, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_END, verboseHandlerConcurrentCollectionEnd, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_ABORTED, verboseHandlerConcurrentAborted, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_REMEMBERED_SET_SCAN_END, verboseHandlerConcurrentRememberedSetScanEnd, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COMPLETE_TRACING_END, verboseHandlerConcurrentTracingEnd, NULL);
	(*_mmPrivateHooks)->J9HookUnregister(_mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_CARD_CLEANING_END, verboseHandlerConcurrentCardCleaningEnd, NULL);
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

	/* Excessive GC */
	(*_mmOmrHooks)->J9HookUnregister(_mmOmrHooks, J9HOOK_MM_OMR_EXCESSIVEGC_RAISED, verboseHandlerExcessiveGCRaised, NULL);
}

const char *
MM_VerboseHandlerOutputStandard::getCycleType(uintptr_t type)
{
	const char* cycleType = NULL;
	switch (type) {
	case OMR_GC_CYCLE_TYPE_DEFAULT:
		cycleType = "default";
		break;
	case OMR_GC_CYCLE_TYPE_GLOBAL:
		cycleType = "global";
		break;
	case OMR_GC_CYCLE_TYPE_SCAVENGE:
		cycleType = "scavenge";
		break;
	default:
		cycleType = "unknown";
		break;
	}

	return cycleType;
}

void
MM_VerboseHandlerOutputStandard::handleGCOPStanza(MM_EnvironmentBase* env, const char *type, uintptr_t contextID, uint64_t duration, bool deltaTimeSuccess)
{
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	if (!deltaTimeSuccess) {
		writer->formatAndOutput(env, 0, "<warning details=\"clock error detected, following timing may be inaccurate\" />");
	}

	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), type ,contextID, duration, omrtime_current_time_millis());
	writer->formatAndOutput(env, 0, "<gc-op %s />", tagTemplate);
	writer->flush(env);
}

void
MM_VerboseHandlerOutputStandard::handleGCOPOuterStanzaStart(MM_EnvironmentBase* env, const char *type, uintptr_t contextID, uint64_t duration, bool deltaTimeSuccess)
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
MM_VerboseHandlerOutputStandard::handleGCOPOuterStanzaEnd(MM_EnvironmentBase* env)
{
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	writer->formatAndOutput(env, 0, "</gc-op>");
}

void
MM_VerboseHandlerOutputStandard::handleMarkEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_MarkEndEvent* event = (MM_MarkEndEvent*)eventData;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_MarkStats *markStats = &extensions->globalGCStats.markStats;
	uint64_t duration = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&duration, markStats->_startTime, markStats->_endTime);

	enterAtomicReportingBlock();
	handleGCOPOuterStanzaStart(env, "mark", env->_cycleState->_verboseContextID, duration, deltaTimeSuccess);

	writer->formatAndOutput(env, 1, "<trace-info objectcount=\"%zu\" scancount=\"%zu\" scanbytes=\"%zu\" />",
			markStats->_objectsMarked, markStats->_objectsScanned, markStats->_bytesScanned);

	handleMarkEndInternal(env, eventData);

	handleGCOPOuterStanzaEnd(env);
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleMarkEndInternal(MM_EnvironmentBase* env, void* eventData)
{
	/* Empty stub */
}

void
MM_VerboseHandlerOutputStandard::handleSweepEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_SweepEndEvent* event = (MM_SweepEndEvent*)eventData;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	MM_SweepStats *sweepStats = &extensions->globalGCStats.sweepStats;
	uint64_t duration = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&duration, sweepStats->_startTime, sweepStats->_endTime);

	enterAtomicReportingBlock();
	handleGCOPStanza(env, "sweep", env->_cycleState->_verboseContextID, duration, deltaTimeSuccess);

	handleSweepEndInternal(env, eventData);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleSweepEndInternal(MM_EnvironmentBase* env, void* eventData)
{
	/* Empty stub */
}

#if defined(OMR_GC_MODRON_COMPACTION)

void
MM_VerboseHandlerOutputStandard::handleCompactStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_CompactEndEvent* event = (MM_CompactEndEvent*)eventData;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->omrVMThread);
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();

	MM_CollectionStatisticsStandard *stats = (MM_CollectionStatisticsStandard *)env->_cycleState->_collectionStatistics;
	if (NO_FRAGMENTATION != stats->_tenureFragmentation) {
		stats->collectCollectionStatistics(env, stats);
		enterAtomicReportingBlock();
		outputMemoryInfo(env, manager->getIndentLevel(), stats);
		writer->flush(env);
		exitAtomicReportingBlock();
		stats->resetFragmentionStats(env);
	}
}

void
MM_VerboseHandlerOutputStandard::handleCompactEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_CompactEndEvent* event = (MM_CompactEndEvent*)eventData;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->omrVMThread);
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_CompactStats *compactStats = &MM_GCExtensionsBase::getExtensions(env->getOmrVM())->globalGCStats.compactStats;
	uint64_t duration = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&duration, compactStats->_startTime, compactStats->_endTime);

	enterAtomicReportingBlock();
	handleGCOPOuterStanzaStart(env, "compact", env->_cycleState->_verboseContextID, duration, deltaTimeSuccess);

	if(COMPACT_PREVENTED_NONE == compactStats->_compactPreventedReason) {
		writer->formatAndOutput(env, 1, "<compact-info movecount=\"%zu\" movebytes=\"%zu\" reason=\"%s\" />",
				compactStats->_movedObjects, compactStats->_movedBytes, getCompactionReasonAsString(compactStats->_compactReason));
	} else {
		writer->formatAndOutput(env, 1, "<compact-info reason=\"%s\" />", getCompactionReasonAsString(compactStats->_compactReason));
		writer->formatAndOutput(env, 1, "<warning details=\"compaction prevented due to %s\" />", getCompactionPreventedReasonAsString(compactStats->_compactPreventedReason));
	}

	handleCompactEndInternal(env, eventData);

	handleGCOPOuterStanzaEnd(env);
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleCompactEndInternal(MM_EnvironmentBase* env, void* eventData)
{
	/* Empty stub */
}

#endif /* defined(OMR_GC_MODRON_COMPACTION) */

#if defined(OMR_GC_MODRON_SCAVENGER)
void
MM_VerboseHandlerOutputStandard::handleScavengeEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ScavengeEndEvent* event = (MM_ScavengeEndEvent*)eventData;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_ScavengerStats *scavengerStats = &extensions->scavengerStats;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t duration = 0;
	bool deltaTimeSuccess = getTimeDeltaInMicroSeconds(&duration, scavengerStats->_startTime, scavengerStats->_endTime);

	enterAtomicReportingBlock();
	handleGCOPOuterStanzaStart(env, "scavenge", env->_cycleState->_verboseContextID, duration, deltaTimeSuccess);

	writer->formatAndOutput(env, 1, "<scavenger-info tenureage=\"%zu\" tenuremask=\"%4zx\" tiltratio=\"%zu\" />", scavengerStats->_tenureAge, scavengerStats->getFlipHistory(0)->_tenureMask, scavengerStats->_tiltRatio);

	if (0 != scavengerStats->_flipCount) {
		writer->formatAndOutput(env, 1, "<memory-copied type=\"nursery\" objects=\"%zu\" bytes=\"%zu\" bytesdiscarded=\"%zu\" />",
				scavengerStats->_flipCount, scavengerStats->_flipBytes, scavengerStats->_flipDiscardBytes);
	}
	if (0 != scavengerStats->_tenureAggregateCount) {
		writer->formatAndOutput(env, 1, "<memory-copied type=\"tenure\" objects=\"%zu\" bytes=\"%zu\" bytesdiscarded=\"%zu\" />",
				scavengerStats->_tenureAggregateCount, scavengerStats->_tenureAggregateBytes, scavengerStats->_tenureDiscardBytes);
	}
	if (0 != scavengerStats->_failedFlipCount) {
		writer->formatAndOutput(env, 1, "<copy-failed type=\"nursery\" objects=\"%zu\" bytes=\"%zu\" />",
				scavengerStats->_failedFlipCount, scavengerStats->_failedFlipBytes);
	}
	if (0 != scavengerStats->_failedTenureCount) {
		writer->formatAndOutput(env, 1, "<copy-failed type=\"tenure\" objects=\"%zu\" bytes=\"%zu\" />",
				scavengerStats->_failedTenureCount, scavengerStats->_failedTenureBytes);
	}

	handleScavengeEndInternal(env, eventData);
	
	if(0 != scavengerStats->_tenureExpandedCount) {
		uint64_t expansionMicros = omrtime_hires_delta(0, scavengerStats->_tenureExpandedTime, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
		outputCollectorHeapResizeInfo(env, 1, HEAP_EXPAND, scavengerStats->_tenureExpandedBytes, scavengerStats->_tenureExpandedCount, MEMORY_TYPE_OLD, SATISFY_COLLECTOR, expansionMicros);
	}

	if(scavengerStats->_rememberedSetOverflow) {
		writer->formatAndOutput(env, 1, "<warning details=\"remembered set overflow detected\" />");
		if(scavengerStats->_causedRememberedSetOverflow) {
			writer->formatAndOutput(env, 1, "<warning details=\"remembered set overflow triggered\" />");
		}
	}
	if(scavengerStats->_scanCacheOverflow) {
		writer->formatAndOutput(env, 1, "<warning details=\"scan cache overflow (new chunk allocation acquired durationms=%zu, fromHeap=%s)\" />", scavengerStats->_scanCacheAllocationDurationDuringSavenger, (0 != scavengerStats->_scanCacheAllocationFromHeap)?"true":"false");
	}
	if(scavengerStats->_backout) {
		writer->formatAndOutput(env, 1, "<warning details=\"aborted collection due to insufficient free space\" />");
	}

	handleGCOPOuterStanzaEnd(env);
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleScavengeEndInternal(MM_EnvironmentBase* env, void* eventData)
{
	/* Empty stub */
}

void
MM_VerboseHandlerOutputStandard::handleScavengePercolate(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_PercolateCollectEvent *event = (MM_PercolateCollectEvent *)eventData;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();

	char tagTemplate[200];
	getTagTemplate(tagTemplate, sizeof(tagTemplate), omrtime_current_time_millis());
	enterAtomicReportingBlock();
	writer->formatAndOutput(env, 0, "<percolate-collect id=\"%zu\" from=\"%s\" to=\"%s\" reason=\"%s\" %s/>", manager->getIdAndIncrement(), "nursery", "global", getPercolateReasonAsString((PercolateReason)event->reason), tagTemplate);
	writer->flush(env);

	handleScavengePercolateInternal(env, eventData);

	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleScavengePercolateInternal(MM_EnvironmentBase* env, void* eventData)
{
	/* Empty stub */
}
#endif /*defined(OMR_GC_MODRON_SCAVENGER) */

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
void
MM_VerboseHandlerOutputStandard::handleConcurrentRememberedSetScanEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ConcurrentRememberedSetScanEndEvent* event = (MM_ConcurrentRememberedSetScanEndEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uint64_t durationUs = omrtime_hires_delta(0, event->duration, OMRPORT_TIME_DELTA_IN_MICROSECONDS);

	enterAtomicReportingBlock();
	handleGCOPOuterStanzaStart(env, "rs-scan", env->_cycleState->_verboseContextID, durationUs, true);

	writer->formatAndOutput(
			env, 1, "<scan objectsFound=\"%zu\" bytesTraced=\"%zu\" workStackOverflowCount=\"%zu\" />",
			event->objectsFound, event->bytesTraced, event->workStackOverflowCount);

	handleConcurrentRememberedSetScanEndInternal(env, eventData);

	handleGCOPOuterStanzaEnd(env);
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentRememberedSetScanEndInternal(MM_EnvironmentBase *env, void *eventData)
{
	/* Empty stub */
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentCardCleaningEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ConcurrentCollectionCardCleaningEndEvent* event = (MM_ConcurrentCollectionCardCleaningEndEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uint64_t durationUs = omrtime_hires_delta(0, event->duration, OMRPORT_TIME_DELTA_IN_MICROSECONDS);

	enterAtomicReportingBlock();
	handleGCOPOuterStanzaStart(env, "card-cleaning", env->_cycleState->_verboseContextID, durationUs, true);

	writer->formatAndOutput(
			env, 1, "<card-cleaning cardsCleaned=\"%zu\" bytesTraced=\"%zu\" workStackOverflowCount=\"%zu\" />",
			event->finalcleanedCards, event->bytesTraced, event->workStackOverflowCount);

	handleConcurrentCardCleaningEndInternal(env, eventData);

	handleGCOPOuterStanzaEnd(env);
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentCardCleaningEndInternal(MM_EnvironmentBase *env, void* eventData)
{
	/* Empty stub */
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentTracingEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ConcurrentCompleteTracingEndEvent* event = (MM_ConcurrentCompleteTracingEndEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uint64_t durationUs = omrtime_hires_delta(0, event->duration, OMRPORT_TIME_DELTA_IN_MICROSECONDS);

	enterAtomicReportingBlock();
	handleGCOPOuterStanzaStart(env, "tracing", env->_cycleState->_verboseContextID, durationUs, true);

	writer->formatAndOutput(env, 1, "<trace bytesTraced=\"%zu\" workStackOverflowCount=\"%zu\" />", event->bytesTraced, event->workStackOverflowCount);

	handleConcurrentTracingEndInternal(env, eventData);

	handleGCOPOuterStanzaEnd(env);
	writer->flush(env);
	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentTracingEndInternal(MM_EnvironmentBase *env, void* eventData)
{
	/* Empty stub */
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentKickoff(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ConcurrentKickoffEvent* event = (MM_ConcurrentKickoffEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	enterAtomicReportingBlock();
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), omrtime_current_time_millis());
	writer->formatAndOutput(env, 0, "<concurrent-kickoff %s>", tagTemplate);

	const char* reasonString = getConcurrentKickoffReason(eventData);

	if (extensions->isScavengerEnabled()) {
		writer->formatAndOutput(
				env, 1, "<kickoff reason=\"%s\" targetBytes=\"%zu\" thresholdFreeBytes=\"%zu\"  remainingFree=\"%zu\" tenureFreeBytes=\"%zu\" nurseryFreeBytes=\"%zu\" />",
				reasonString, event->traceTarget, event->kickOffThreshold, event->remainingFree, event->commonData->tenureFreeBytes, event->commonData->nurseryFreeBytes);
	} else {
		writer->formatAndOutput(
				env, 1, "<kickoff reason=\"%s\" targetBytes=\"%zu\" thresholdFreeBytes=\"%zu\" remainingFree=\"%zu\" tenureFreeBytes=\"%zu\" />",
				reasonString, event->traceTarget, event->kickOffThreshold, event->remainingFree, event->commonData->tenureFreeBytes);
	}
	writer->formatAndOutput(env, 0, "</concurrent-kickoff>");
	writer->flush(env);

	handleConcurrentKickoffInternal(env, eventData);

	exitAtomicReportingBlock();
}

const char*
MM_VerboseHandlerOutputStandard::getConcurrentKickoffReason(void *eventData)
{
	MM_ConcurrentKickoffEvent* event = (MM_ConcurrentKickoffEvent*)eventData;
	const char* reasonString;
	switch ((ConcurrentKickoffReason)event->reason) {
	case KICKOFF_THRESHOLD_REACHED:
		reasonString = "threshold reached";
		break;
	case NEXT_SCAVENGE_WILL_PERCOLATE:
		reasonString = "next scavenge will percolate";
		break;
	case NO_KICKOFF_REASON:
		/* Should never be the case */
		reasonString = "none";
		break;
	default:
		reasonString = "unknown";
		break;
	}
	return reasonString;
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentKickoffInternal(MM_EnvironmentBase *env, void* eventData)
{
	/* Empty stub */
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentHalted(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ConcurrentHaltedEvent* event = (MM_ConcurrentHaltedEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	char tagTemplate[200];
	enterAtomicReportingBlock();
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), omrtime_current_time_millis());
	writer->formatAndOutput(env, 0, "<concurrent-halted %s>", tagTemplate);

	handleConcurrentHaltedInternal(env, eventData);

	writer->formatAndOutput(env, 1,
			"<traced "
			"bytesTarget=\"%zu\" bytesTotal=\"%zu\" "
			"bytesByMutator=\"%zu\" bytesByHelper=\"%zu\" "
			"percent=\"%zu\" />",
			event->traceTarget, event->tracedTotal,
			event->tracedByMutators, event->tracedByHelpers,
			event->traceTarget == 0 ? 0 : (uintptr_t)(((uint64_t)event->tracedTotal * 100) / (uint64_t)event->traceTarget));
	writer->formatAndOutput(env, 1, "<cards cleaned=\"%zu\" thresholdBytes=\"%zu\" />", event->cardsCleaned, event->cardCleaningThreshold);
	writer->formatAndOutput(env, 0, "</concurrent-halted>");
	writer->flush(env);

	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentHaltedInternal(MM_EnvironmentBase *env, void* eventData)
{
	MM_ConcurrentHaltedEvent* event = (MM_ConcurrentHaltedEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
#define CONCURRENT_STATUS_BUFFER_LENGTH 32
	char statusBuffer[CONCURRENT_STATUS_BUFFER_LENGTH];
	const char* statusString = MM_ConcurrentGCStats::getConcurrentStatusString(env, event->executionMode, statusBuffer, CONCURRENT_STATUS_BUFFER_LENGTH);
#undef CONCURRENT_STATUS_BUFFER_LENGTH
	const char* stateString = "Complete";
	if (0 == event->isTracingExhausted) {
		stateString = "Tracing incomplete";
	}

	writer->formatAndOutput(env, 1, "<halted state=\"%s\" status=\"%s\" />", stateString, statusString);
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentCollectionStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ConcurrentCollectionStartEvent* event = (MM_ConcurrentCollectionStartEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uint64_t currentTime = event->timestamp;
	uint64_t previousTime = manager->getLastConcurrentGCTime();
	manager->setLastConcurrentGCTime(currentTime);
	if (0 == previousTime) {
		previousTime = manager->getInitializedTime();
	}
	uint64_t deltaTime = omrtime_hires_delta(previousTime, currentTime, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
	const char* cardCleaningReasonString = "unknown";

	switch (event->cardCleaningReason) {
	case TRACING_COMPLETED:
		cardCleaningReasonString = "tracing completed";
		break;
	case CARD_CLEANING_THRESHOLD_REACHED:
		cardCleaningReasonString = "card cleaning threshold reached";
		break;
	}

	char tagTemplate[200];
	enterAtomicReportingBlock();
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), omrtime_current_time_millis());
	writer->formatAndOutput(env, 0, "<concurrent-collection-start %s intervalms=\"%llu.%03llu\" >",
		tagTemplate, deltaTime / 1000, deltaTime % 1000);
	writer->formatAndOutput(env, 1, "<concurrent-trace-info reason=\"%s\" tracedByMutators=\"%zu\" tracedByHelpers=\"%zu\" cardsCleaned=\"%zu\" workStackOverflowCount=\"%zu\" />",
		cardCleaningReasonString, event->tracedByMutators, event->tracedByHelpers, event->cardsCleaned, event->workStackOverflowCount);
  	writer->formatAndOutput(env, 0, "</concurrent-collection-start>");

	writer->flush(env);

	handleConcurrentCollectionStartInternal(env, eventData);

	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentCollectionStartInternal(MM_EnvironmentBase *env, void* eventData)
{
	/* Empty stub */
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentCollectionEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ConcurrentCollectionEndEvent* event = (MM_ConcurrentCollectionEndEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[200];
	enterAtomicReportingBlock();
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), omrtime_current_time_millis());
	writer->formatAndOutput(env, 0, "<concurrent-collection-end %s />", tagTemplate);
	writer->flush(env);

	handleConcurrentCollectionEndInternal(env, eventData);

	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentCollectionEndInternal(MM_EnvironmentBase *env, void* eventData)
{
	/* Empty stub */
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentAborted(J9HookInterface** hook, uintptr_t eventNum, void* eventData)
{
	MM_ConcurrentAbortedEvent* event = (MM_ConcurrentAbortedEvent*)eventData;
	MM_VerboseManager* manager = getManager();
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	char tagTemplate[100];
	enterAtomicReportingBlock();
	getTagTemplate(tagTemplate, sizeof(tagTemplate), manager->getIdAndIncrement(), omrtime_current_time_millis());
	writer->formatAndOutput(env, 0, "<concurrent-aborted %s>", tagTemplate);

	const char* reason;
	switch((CollectionAbortReason)event->reason) {
	case ABORT_COLLECTION_INSUFFICENT_PROGRESS:
		reason = "insufficient progress made";
		break;
	case ABORT_COLLECTION_REMEMBERSET_OVERFLOW:
		reason = "remembered set overflow";
		break;
	case ABORT_COLLECTION_SCAVENGE_REMEMBEREDSET_OVERFLOW:
		reason = "scavenge remembered set overflow";
		break;
	case ABORT_COLLECTION_PREPARE_HEAP_FOR_WALK:
		reason = "prepare heap for walk";
		break;
	default:
		reason = "unknown";
		break;
	}

	writer->formatAndOutput(env, 1, "<reason value=\"%s\" />", reason);
	writer->formatAndOutput(env, 0, "</concurrent-aborted>");
	writer->flush(env);

	handleConcurrentAbortedInternal(env, eventData);

	exitAtomicReportingBlock();
}

void
MM_VerboseHandlerOutputStandard::handleConcurrentAbortedInternal(MM_EnvironmentBase *env, void *eventData)
{
	/* Empty stub */
}
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

bool
MM_VerboseHandlerOutputStandard::hasOutputMemoryInfoInnerStanza()
{
	return true;
}

void
MM_VerboseHandlerOutputStandard::outputMemType(MM_EnvironmentBase* env, uintptr_t indent, const char* type, uintptr_t free, uintptr_t total, uint32_t tenureFragmentation, uintptr_t microFragment, uintptr_t macroFragment)
{
	char memInfoBuffer[INITIAL_BUFFER_SIZE] = "";
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);

	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	uintptr_t bufPos = 0;

	bufPos += omrstr_printf(memInfoBuffer, INITIAL_BUFFER_SIZE, "<mem type=\"%s\" free=\"%zu\" total=\"%zu\" percent=\"%zu\"",
			type, (size_t) free, (size_t) total, (size_t) ((total == 0) ? 0 : ((uintptr_t)(((uint64_t)free*100) / (uint64_t)total))));
	if (MICRO_FRAGMENTATION == (MICRO_FRAGMENTATION & tenureFragmentation)) {
		bufPos += omrstr_printf(memInfoBuffer + bufPos, INITIAL_BUFFER_SIZE - bufPos, " micro-fragmented=\"%zu\"", (size_t) microFragment);
	}
	if (MACRO_FRAGMENTATION == (MACRO_FRAGMENTATION & tenureFragmentation)) {
		bufPos += omrstr_printf(memInfoBuffer + bufPos, INITIAL_BUFFER_SIZE - bufPos," macro-fragmented=\"%zu\"", (size_t) macroFragment);
	}
	bufPos += omrstr_printf(memInfoBuffer + bufPos, INITIAL_BUFFER_SIZE - bufPos, " />");
	writer->formatAndOutput(env, indent, memInfoBuffer);
}

void
MM_VerboseHandlerOutputStandard::outputMemoryInfoInnerStanza(MM_EnvironmentBase *env, uintptr_t indent, MM_CollectionStatistics *statsBase)
{
	MM_VerboseWriterChain* writer = _manager->getWriterChain();
	MM_CollectionStatisticsStandard *stats = MM_CollectionStatisticsStandard::getCollectionStatistics(statsBase);

	if (stats->_scavengerEnabled) {
		writer->formatAndOutput(env, indent, "<mem type=\"nursery\" free=\"%zu\" total=\"%zu\" percent=\"%zu\">",
				stats->_totalFreeNurseryHeapSize, stats->_totalNurseryHeapSize,
				((stats->_totalNurseryHeapSize == 0) ? 0 : ((uintptr_t)(((uint64_t)stats->_totalFreeNurseryHeapSize*100) / (uint64_t)stats->_totalNurseryHeapSize))));

		outputMemType(env, indent + 1, "allocate", stats->_totalFreeNurseryHeapSize - stats->_totalFreeSurvivorHeapSize, stats->_totalNurseryHeapSize - stats->_totalSurvivorHeapSize);
		outputMemType(env, indent + 1, "survivor",  stats->_totalFreeSurvivorHeapSize,  stats->_totalSurvivorHeapSize);
		writer->formatAndOutput(env, indent, "</mem>");
	}

	if (stats->_loaEnabled) {
		char tenureMemInfoBuffer[INITIAL_BUFFER_SIZE] = "";
		uintptr_t bufPos = 0;
		OMRPORT_ACCESS_FROM_OMRVM(_omrVM);

		bufPos += omrstr_printf(tenureMemInfoBuffer, INITIAL_BUFFER_SIZE, "<mem type=\"tenure\" free=\"%zu\" total=\"%zu\" percent=\"%zu\"",
				(size_t) stats->_totalFreeTenureHeapSize, (size_t) stats->_totalTenureHeapSize,
				(size_t) ((stats->_totalTenureHeapSize == 0) ? 0 : ((uintptr_t)(((uint64_t)stats->_totalFreeTenureHeapSize*100) / (uint64_t)stats->_totalTenureHeapSize))));
		if (MICRO_FRAGMENTATION == (MICRO_FRAGMENTATION & stats->_tenureFragmentation)) {
			bufPos += omrstr_printf(tenureMemInfoBuffer + bufPos, INITIAL_BUFFER_SIZE - bufPos, " micro-fragmented=\"%zu\"", (size_t) stats->_microFragmentedSize);
		}
		if (MACRO_FRAGMENTATION == (MACRO_FRAGMENTATION & stats->_tenureFragmentation)) {
			bufPos += omrstr_printf(tenureMemInfoBuffer + bufPos, INITIAL_BUFFER_SIZE - bufPos, " macro-fragmented=\"%zu\"", (size_t) stats->_macroFragmentedSize);
		}
		bufPos += omrstr_printf(tenureMemInfoBuffer + bufPos, INITIAL_BUFFER_SIZE - bufPos, ">");
		writer->formatAndOutput(env, indent, tenureMemInfoBuffer);

		outputMemType(env, indent + 1, "soa", (stats->_totalFreeTenureHeapSize - stats->_totalFreeLOAHeapSize), (stats->_totalTenureHeapSize - stats->_totalLOAHeapSize));
		outputMemType(env, indent + 1, "loa", stats->_totalFreeLOAHeapSize, stats->_totalLOAHeapSize);
		writer->formatAndOutput(env, indent, "</mem>");
	} else {
		outputMemType(env, indent, "tenure", stats->_totalFreeTenureHeapSize, stats->_totalTenureHeapSize, stats->_tenureFragmentation, stats->_microFragmentedSize, stats->_macroFragmentedSize);
	}

	outputMemoryInfoInnerStanzaInternal(env, indent, statsBase);

	if (stats->_scavengerEnabled) {
		writer->formatAndOutput(env, indent, "<remembered-set count=\"%zu\" />", stats->_rememberedSetCount);
	}
}

void
MM_VerboseHandlerOutputStandard::outputMemoryInfoInnerStanzaInternal(MM_EnvironmentBase *env, uintptr_t indent, MM_CollectionStatistics *statsBase)
{
	/* Empty stub */
}

const char *
MM_VerboseHandlerOutputStandard::getSubSpaceType(uintptr_t typeFlags)
{
	const char *subSpaceType = NULL;
	if (MEMORY_TYPE_OLD == typeFlags) {
		subSpaceType = "tenure";
	} else if (MEMORY_TYPE_NEW == typeFlags) {
		subSpaceType = "nursery";
	} else {
		subSpaceType = "default";
	}
	return subSpaceType;
}

void
verboseHandlerGCStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleGCStart(hook, eventNum, eventData);
}

void
verboseHandlerGCEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleGCEnd(hook, eventNum, eventData);
}

void
verboseHandlerCycleStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleCycleStart(hook, eventNum, eventData);
}

void
verboseHandlerCycleEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleCycleEnd(hook, eventNum, eventData);
}

void
verboseHandlerExclusiveStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleExclusiveStart(hook, eventNum, eventData);
}

void
verboseHandlerExclusiveEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleExclusiveEnd(hook, eventNum, eventData);
}

void
verboseHandlerSystemGCStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleSystemGCStart(hook, eventNum, eventData);
}

void
verboseHandlerSystemGCEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleSystemGCEnd(hook, eventNum, eventData);
}

void
verboseHandlerAllocationFailureStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleAllocationFailureStart(hook, eventNum, eventData);
}

void
verboseHandlerFailedAllocationCompleted(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleFailedAllocationCompleted(hook, eventNum, eventData);
}

void
verboseHandlerAllocationFailureEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleAllocationFailureEnd(hook, eventNum, eventData);
}

void
verboseHandlerAcquiredExclusiveToSatisfyAllocation(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleAcquiredExclusiveToSatisfyAllocation(hook, eventNum, eventData);
}

void
verboseHandlerMarkEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleMarkEnd(hook, eventNum, eventData);
}

void
verboseHandlerSweepEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleSweepEnd(hook, eventNum, eventData);
}

#if defined(OMR_GC_MODRON_COMPACTION)
void
verboseHandlerCompactStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleCompactStart(hook, eventNum, eventData);
}

void
verboseHandlerCompactEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleCompactEnd(hook, eventNum, eventData);
}
#endif /*OMR_GC_MODRON_COMPACTION*/

#if defined(OMR_GC_MODRON_SCAVENGER)
void
verboseHandlerScavengeEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleScavengeEnd(hook, eventNum, eventData);
}

void
verboseHandlerScavengePercolate(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleScavengePercolate(hook, eventNum, eventData);
}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
void
verboseHandlerConcurrentRememberedSetScanEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleConcurrentRememberedSetScanEnd(hook, eventNum, eventData);
}

void
verboseHandlerConcurrentCardCleaningEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleConcurrentCardCleaningEnd(hook, eventNum, eventData);
}

void
verboseHandlerConcurrentTracingEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleConcurrentTracingEnd(hook, eventNum, eventData);
}

void
verboseHandlerConcurrentKickoff(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleConcurrentKickoff(hook, eventNum, eventData);
}

void
verboseHandlerConcurrentHalted(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleConcurrentHalted(hook, eventNum, eventData);
}

void
verboseHandlerConcurrentCollectionStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleConcurrentCollectionStart(hook, eventNum, eventData);
}

void
verboseHandlerConcurrentCollectionEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleConcurrentCollectionEnd(hook, eventNum, eventData);
}

void
verboseHandlerConcurrentAborted(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutputStandard *)userData)->handleConcurrentAborted(hook, eventNum, eventData);
}
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

void
verboseHandlerExcessiveGCRaised(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	((MM_VerboseHandlerOutput *)userData)->handleExcessiveGCRaised(hook, eventNum, eventData);
}
