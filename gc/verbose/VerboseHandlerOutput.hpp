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

#if !defined(VERBOSEHANDLEROUTPUT_HPP_)
#define VERBOSEHANDLEROUTPUT_HPP_

#include "omrcfg.h"
#include "omr.h"

#include "Base.hpp"

#include "modronbase.h"

class MM_CollectionStatistics;
class MM_EnvironmentBase;
class MM_GCExtensionsBase;
class MM_VerboseManager;

class MM_VerboseHandlerOutput : public MM_Base
{
private:
protected:
	MM_GCExtensionsBase *_extensions;
	OMR_VM *_omrVM;
	J9HookInterface** _mmPrivateHooks;  /**< Pointers to the internal Hook interface */
	J9HookInterface** _mmOmrHooks;  /**< Pointers to the internal Hook interface */
	MM_VerboseManager *_manager; /* VerboseManager used to format and print output */
public:

private:
protected:
	MM_VerboseHandlerOutput(MM_GCExtensionsBase *extensions);

	virtual bool initialize(MM_EnvironmentBase *env, MM_VerboseManager *manager);
	virtual void tearDown(MM_EnvironmentBase *env);

	virtual bool getThreadName(char *buf, uintptr_t bufLen, OMR_VMThread *vmThread);
	virtual void writeVmArgs(MM_EnvironmentBase* env);

	bool getTimeDeltaInMicroSeconds(uint64_t *timeInMicroSeconds, uint64_t startTime, uint64_t endTime)
	{
		if(endTime < startTime) {
			*timeInMicroSeconds = 0;
			return false;
		}
		OMRPORT_ACCESS_FROM_OMRVM(_omrVM);

		*timeInMicroSeconds = omrtime_hires_delta(startTime, endTime, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
		return true;
	}

	/**
	 * Return the time elapsed between startTimeInMicroseconds and endTimeInMicroseconds
	 * @param [out] timeInMicroSeconds Pointer to the variable where the delta will be stored in
	 * @param [in] startTimeInMicroseconds start time
	 * @param [in] endTimeInMicroseconds end time
	 * @return true if the calculation was successfull. false, otherwise - timeInMicroSeconds is set to 0.
	 */
	bool
	getTimeDelta(uint64_t *timeInMicroSeconds, uint64_t startTimeInMicroseconds, uint64_t endTimeInMicroseconds)
	{
		if(endTimeInMicroseconds < startTimeInMicroseconds) {
			*timeInMicroSeconds = 0;
			return false;
		}

		*timeInMicroSeconds = endTimeInMicroseconds - startTimeInMicroseconds;
		return true;
	}

	/**
	 * Answer a string representation of the current cycle type.
	 * @param env current GC thread.
	 * @return string representing the human readable "type" of the cycle.
	 */
	const char *getCurrentCycleType(MM_EnvironmentBase *env);
	/**
	 * Answer a string representation of a given cycle type.
	 * @param[IN] cycle type
	 * @return string representing the human readable "type" of the cycle.
	 */	
	virtual const char *getCycleType(uintptr_t type);


	/**
	 * Handle any output or data tracking for the initialized phase of verbose GC.
	 * This routine is meant to be overridden by subclasses to implement collector specific output or functionality.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	virtual void handleInitializedInnerStanzas(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Handle any output or data tracking for the initialized phase of verbose GC.
	 * This routing handles region specific information.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleInitializedRegion(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Determine if the receive has inner stanza details for cycle start events.
	 * @return a flag indicating if further output is required.
	 */
	virtual bool hasCycleStartInnerStanzas();

	/**
	 * Handle any output or data tracking for the cycle start event of verbose GC.
	 * This routine is meant to be overridden by subclasses to implement collector specific output or functionality.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 * @param indentDepth base indent count for all inner stanza lines.
	 */
	virtual void handleCycleStartInnerStanzas(J9HookInterface** hook, uintptr_t eventNum, void* eventData, uintptr_t indentDepth);

	/**
	 * Determine if the receive has inner stanza details for cycle end events.
	 * @return a flag indicating if further output is required.
	 */
	virtual bool hasCycleEndInnerStanzas();

	/**
	 * Handle any output or data tracking for the cycle end event of verbose GC.
	 * This routine is meant to be overridden by subclasses to implement collector specific output or functionality.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 * @param indentDepth base indent count for all inner stanza lines.
	 */
	virtual void handleCycleEndInnerStanzas(J9HookInterface** hook, uintptr_t eventNum, void* eventData, uintptr_t indentDepth);

	/**
	 * Determine if the receive has inner stanza details for cycle end events.
	 * @return a flag indicating if further output is required.
	 */
	virtual bool hasAllocationFailureStartInnerStanzas();

	/**
	 * Handle any output or data tracking for the allocation failure start event of verbose GC.
	 * This routine is meant to be overridden by subclasses to implement collector specific output or functionality.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 * @param indentDepth base indent count for all inner stanza lines.
	 */
	virtual void handleAllocationFailureStartInnerStanzas(J9HookInterface** hook, uintptr_t eventNum, void* eventData, uintptr_t indentDepth);

	/* Print out allocations statistics
	 * @param current Env
	 */
	virtual void printAllocationStats(MM_EnvironmentBase* env);

	/**
	 * Called before outputting verbose data which is intended to be logically atomic.  Most implementations do nothing with this
	 * call but some might need to lock if they permit concurrent event reporting.
	 */
	virtual void enterAtomicReportingBlock();

	/**
	 * Called after outputting verbose data which is intended to be logically atomic.  Most implementations do nothing with this
	 * call but some might need to unlock if they permit concurrent event reporting.
	 */
	virtual void exitAtomicReportingBlock();

	/**
	 * Output a stand-alone stanza on memory statistics related to the heap.
	 * @param env GC thread used for output.
	 * @param indent base level of indentation for the summary.
	 * @param commonData Structure which contains the memory information.
	 */
	void outputMemoryInfo(MM_EnvironmentBase *env, uintptr_t indent, MM_CollectionStatistics *stats);

	virtual bool hasOutputMemoryInfoInnerStanza();

	virtual void outputMemoryInfoInnerStanza(MM_EnvironmentBase *env, uintptr_t indent, MM_CollectionStatistics *stats);

	/**
	 * Output a stand-alone stanza heap resize events.
	 * @param env GC thread used for output.
	 * @param indent base level of indentation for the summary.
	 * @param resizeType type of resize (ie compact or expand)
	 * @param resizeAmount the amount of the the resize in bytes
	 * @param resizeCount the count of how many resizes took place
	 * @param subSpaceType the subpsace in which the resize took place
	 * @param reason the reason for the resize
	 * @param timeInMicroSeconds the total time that all of the resizes took
	 */
	void outputHeapResizeInfo(MM_EnvironmentBase *env, uintptr_t indent, HeapResizeType resizeType, uintptr_t resizeAmount, uintptr_t resizeCount, uintptr_t subSpaceType, uintptr_t reason, uint64_t timeInMicroSeconds);

	/**
	 * Output an embedded stanza for collector heap resize events.
	 * @param env GC thread used for output.
	 * @param indent base level of indentation for the summary.
	 * @param resizeType type of resize (ie compact or expand)
	 * @param resizeAmount the amount of the the resize in bytes
	 * @param resizeCount the count of how many resizes took place
	 * @param subSpaceType the subpsace in which the resize took place
	 * @param reason the reason for the resize
	 * @param timeInMicroSeconds the total time that all of the resizes took
	 */
	void outputCollectorHeapResizeInfo(MM_EnvironmentBase *env, uintptr_t indent, HeapResizeType resizeType, uintptr_t resizeAmount, uintptr_t resizeCount, uintptr_t subSpaceType, uintptr_t reason, uint64_t timeInMicroSeconds);

	/**
	 * Get the string representation of the subspace type
	 * @param typeFlags the type flags stored for the subpace in question
	 * @return the string representation for the subspace type
	 */
	virtual const char *getSubSpaceType(uintptr_t typeFlags);

	/**
	 * Output string constants table summary.
	 * @param env GC thread used for output.
	 * @param indent base level of indentation for the summary.
	 * @param candidates total count candidate strings considered
	 * @param cleared total count of cleared strings
	 */
	void outputStringConstantInfo(MM_EnvironmentBase *env, uintptr_t indent, uintptr_t candidates, uintptr_t cleared);

public:
	static MM_VerboseHandlerOutput *newInstance(MM_EnvironmentBase *env, MM_VerboseManager *manager);

	void kill(MM_EnvironmentBase *env);

	/**
	 * Perform necessary hooks and infrastructure setup to enable verbose gc output.
	 */
	virtual void enableVerbose();

	/**
	 * Perform necessary unhook and infrastructure teardown to disable verbose gc output.
	 */
	virtual void disableVerbose();

	/**
	 *  Get the VerboseManager used to format and print output
	 *  
	 *  @return _manger the VerboseManager used to format and print output
	 */
	MM_VerboseManager *getManager() {return _manager;}

	/**
	 * Build the standard top level tag template.
	 * @param buf character buffer in which to create to tag template.
	 * @param bufsize maximum size allowed in the character buffer.
	 * @param wallTimeMs wall clock time to be used as the timestamp for the tag.
	 * @return number of bytes consumed in the buffer.
	 *
	 * @note should be moved to protected once all standard usage is converted.
	 */
	uintptr_t getTagTemplate(char *buf, uintptr_t bufsize, uint64_t wallTimeMs);

	/**
	 * Build the standard top level tag template.
	 * @param buf character buffer in which to create to tag template.
	 * @param bufsize maximum size allowed in the character buffer.
	 * @param id unique id of the tag being built.
	 * @param wallTimeMs wall clock time to be used as the timestamp for the tag.
	 * @return number of bytes consumed in the buffer.
	 *
	 * @note should be moved to protected once all standard usage is converted.
	 */
	uintptr_t getTagTemplate(char *buf, uintptr_t bufsize, uintptr_t id, uint64_t wallTimeMs);

	/**
	 * Build the standard top level tag template.
	 * @param buf character buffer in which to create to tag template.
	 * @param bufsize maximum size allowed in the character buffer.
	 * @param id unique id of the tag being built.
	 * @param type Human readable name for the type of the tag.
	 * @param contextId unique identifier of the associated event this is associated with (parent/sibling relationship).
	 * @param wallTimeMs wall clock time to be used as the timestamp for the tag.
	 * @return number of bytes consumed in the buffer.
	 *
	 * @note should be moved to protected once all standard usage is converted.
	 */
	uintptr_t getTagTemplate(char *buf, uintptr_t bufsize, uintptr_t id, const char *type, uintptr_t contextId, uint64_t wallTimeMs);

	/**
	 * Build the standard top level tag template.
	 * @param buf character buffer in which to create to tag template.
	 * @param bufsize maximum size allowed in the character buffer.
	 * @param id unique id of the tag being built.
	 * @param type Human readable name for the type of the tag - old cycle that has finished.
	 * @param type Human readable name for the type of the tag - new cycle that is starting.
	 * @param contextId unique identifier of the associated event this is associated with (parent/sibling relationship).
	 * @param wallTimeMs wall clock time to be used as the timestamp for the tag.
	 * @return number of bytes consumed in the buffer.
	 *
	 * @note should be moved to protected once all standard usage is converted.
	 */
	uintptr_t getTagTemplateWithOldType(char *buf, uintptr_t bufsize, uintptr_t id, const char *oldType, const char *newType, uintptr_t contextId, uint64_t wallTimeMs);

	/**
	 * Build the standard top level tag template.
	 * @param buf character buffer in which to create to tag template.
	 * @param bufsize maximum size allowed in the character buffer.
	 * @param id unique id of the tag being built.
	 * @param type Human readable name for the type of the tag.
	 * @param contextId unique identifier of the associated event this is associated with (parent/sibling relationship).
	 * @param timeus the time in microseconds taken for this piece of work
	 * @param wallTimeMs wall clock time to be used as the timestamp for the tag.
	 * @return number of bytes consumed in the buffer.
	 *
	 * @note should be moved to protected once all standard usage is converted.
	 */
	uintptr_t getTagTemplate(char *buf, uintptr_t bufsize, uintptr_t id, const char *type, uintptr_t contextId, uint64_t timeus, uint64_t wallTimeMs);

	/**
	 * Build the standard top level tag template.
	 * @param buf character buffer in which to create to tag template.
	 * @param bufsize maximum size allowed in the character buffer.
	 * @param id unique id of the tag being built.
	 * @param type Human readable name for the type of the tag.
	 * @param contextId unique identifier of the associated event this is associated with (parent/sibling relationship).
	 * @param durationus the time difference in microseconds between this stanza and another sibling stanza in past (for example from <gc-start to <gc-end)
	 * @param usertimeus the time difference in microseconds taken in user space for this process
	 * @param cputimeus the time difference in microseconds taken in kernel space for this process
	 * @param wallTimeMs wall clock time to be used as the timestamp for the tag.
	 * @return number of bytes consumed in the buffer.
	 *
	 * @note should be moved to protected once all standard usage is converted.
	 */
	uintptr_t getTagTemplateWithDuration(char *buf, uintptr_t bufsize, uintptr_t id, const char *type, uintptr_t contextId, uint64_t durationus, uint64_t usertimeus, uint64_t cputimeus, uint64_t wallTimeMs);

	/**
	 * Handle any output or data tracking for the initialized phase of verbose GC.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	virtual void handleInitialized(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for a cycle start event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleCycleStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	void handleCycleContinue(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for a cycle end event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleCycleEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write the verbose stanza for the exclusive access start event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleExclusiveStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write the verbose stanza for the exclusive access end event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleExclusiveEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write the verbose stanza for the system gc start event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleSystemGCStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write the verbose stanza for the system gc end event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleSystemGCEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write the verbose stanza for the allocation failure start event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleAllocationFailureStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write the verbose stanza for the allocation causing AF completed event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleFailedAllocationCompleted(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write the verbose stanza for the allocation failure end event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleAllocationFailureEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write the verbose stanza for exclusive access acquisition that satisfies an allocation.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleAcquiredExclusiveToSatisfyAllocation(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for a global GC start event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleGCStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write verbose stanza for a global GC end event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleGCEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	void handleHeapResize(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

	/**
	 * Write the verbose stanza for the excessive gc raised event.
	 * @param hook Hook interface used by the JVM.
	 * @param eventNum The hook event number.
	 * @param eventData hook specific event data.
	 */
	void handleExcessiveGCRaised(J9HookInterface** hook, uintptr_t eventNum, void* eventData);

};

#endif /* VERBOSEHANDLEROUTPUT_HPP_ */
