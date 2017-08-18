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

#if !defined(VERBOSEHANDLEROUTPUTSTANDARD_HPP_)
#define VERBOSEHANDLEROUTPUTSTANDARD_HPP_

#include "omrcfg.h"
#include "omrcomp.h"

#include "VerboseHandlerOutput.hpp"
#include "CollectionStatisticsStandard.hpp"

class MM_CollectionStatistics;
class MM_EnvironmentBase;

class MM_VerboseHandlerOutputStandard : public MM_VerboseHandlerOutput
{
private:
protected:
public:

private:

protected:
	void outputMemType(MM_EnvironmentBase* env, uintptr_t indent, const char* type, uintptr_t free, uintptr_t total, uint32_t tenureFragmentation=NO_FRAGMENTATION, uintptr_t microFragment=0, uintptr_t macroFragment=0);
	virtual bool initialize(MM_EnvironmentBase *env, MM_VerboseManager *manager);
	virtual void tearDown(MM_EnvironmentBase *env);

	/**
	 * Answer a string representation of a given cycle type.
	 * @param[IN] cycle type
	 * @return string representing the human readable "type" of the cycle.
	 */	
	virtual const char *getCycleType(uintptr_t type);

	void handleGCOPStanza(MM_EnvironmentBase* env, const char *type, uintptr_t contextID, uint64_t duration, bool deltaTimeSuccess);
	void handleGCOPOuterStanzaStart(MM_EnvironmentBase* env, const char *type, uintptr_t contextID, uint64_t duration, bool deltaTimeSuccess);
	void handleGCOPOuterStanzaEnd(MM_EnvironmentBase* env);

	virtual bool hasOutputMemoryInfoInnerStanza();
	virtual void outputMemoryInfoInnerStanzaInternal(MM_EnvironmentBase *env, uintptr_t indent, MM_CollectionStatistics *stats);
	virtual void outputMemoryInfoInnerStanza(MM_EnvironmentBase *env, uintptr_t indent, MM_CollectionStatistics *stats);
	virtual const char *getSubSpaceType(uintptr_t typeFlags);

	/* Language-extendable internal logic for GC events. */
	virtual void handleMarkEndInternal(MM_EnvironmentBase* env, void* eventData);
	virtual void handleSweepEndInternal(MM_EnvironmentBase* env, void* eventData);
#if defined(OMR_GC_MODRON_COMPACTION)
	virtual void handleCompactEndInternal(MM_EnvironmentBase* env, void* eventData);
#endif /* defined(OMR_GC_MODRON_COMPACTION) */
#if defined(OMR_GC_MODRON_SCAVENGER)
	virtual void handleScavengeEndInternal(MM_EnvironmentBase* env, void* eventData);
	virtual void handleScavengePercolateInternal(MM_EnvironmentBase* env, void* eventData);
#endif /*defined(OMR_GC_MODRON_SCAVENGER) */
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	virtual void handleConcurrentRememberedSetScanEndInternal(MM_EnvironmentBase *env, void* eventData);
	virtual void handleConcurrentCardCleaningEndInternal(MM_EnvironmentBase *env, void* eventData);
	virtual void handleConcurrentTracingEndInternal(MM_EnvironmentBase *env, void* eventData);
	virtual void handleConcurrentKickoffInternal(MM_EnvironmentBase *env, void* eventData);
	virtual const char* getConcurrentKickoffReason(void *eventData);
	virtual void handleConcurrentHaltedInternal(MM_EnvironmentBase *env, void* eventData);
	virtual void handleConcurrentCollectionStartInternal(MM_EnvironmentBase *env, void* eventData);
	virtual void handleConcurrentCollectionEndInternal(MM_EnvironmentBase *env, void* eventData);
	virtual void handleConcurrentAbortedInternal(MM_EnvironmentBase *env, void* eventData);
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

	MM_VerboseHandlerOutputStandard(MM_GCExtensionsBase *extensions) :
		MM_VerboseHandlerOutput(extensions)
	{};

public:
	static MM_VerboseHandlerOutput *newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager);

	virtual void enableVerbose();
	virtual void disableVerbose();

	/**
	 * Write verbose stanza for a mark end event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleMarkEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for a sweep end event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleSweepEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

#if defined(OMR_GC_MODRON_COMPACTION)

	void handleCompactStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData);
	/**
	 * Write verbose stanza for a compact end event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleCompactEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);
#endif /* defined(OMR_GC_MODRON_COMPACTION) */

#if defined(OMR_GC_MODRON_SCAVENGER)
	/**
	 * Write verbose stanza for a scavenge end event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleScavengeEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for a percolate event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleScavengePercolate(J9HookInterface** hook, uintptr_t eventNum, void* eventData);
	
	virtual const char *getConcurrentTypeString() { return "scavenge"; }
	
	virtual void handleConcurrentGCOpEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	/**
	 * Write verbose stanza for concurrent remembered set scan event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleConcurrentRememberedSetScanEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for concurrent card cleaning event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleConcurrentCardCleaningEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for concurrent tracing event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleConcurrentTracingEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for concurrent kickoff event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleConcurrentKickoff(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for concurrent halted event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleConcurrentHalted(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for concurrent collection start event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleConcurrentCollectionStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for concurrent collection end event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleConcurrentCollectionEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for concurrent aborted event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleConcurrentAborted(J9HookInterface** hook, uintptr_t eventNum, void* eventData);
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
};

#endif /* VERBOSEHANDLEROUTPUTSTANDARD_HPP_ */
