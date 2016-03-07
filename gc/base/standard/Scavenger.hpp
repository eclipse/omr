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
#if !defined(SCAVENGER_HPP_)
#define SCAVENGER_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "omrcomp.h"

#include "CollectionStatisticsStandard.hpp"
#include "Collector.hpp"
#include "CycleState.hpp"
#include "GCExtensionsBase.hpp"
 
struct J9HookInterface;
class MM_AllocateDescription;
class MM_CollectorLanguageInterface;
class MM_EnvironmentBase;
class MM_MemoryPool;
class MM_MemorySubSpace;
class MM_MemorySubSpaceSemiSpace;
class MM_PhysicalSubArena;
class MM_SublistPool;
struct OMR_VM;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_Scavenger : public MM_Collector
{
	/*
	 * Data members
	 */
private:
protected:
	MM_GCExtensionsBase *_extensions;
	
	MM_MemorySubSpaceSemiSpace *_activeSubSpace; /**< top level new subspace subject to GC */
	MM_MemorySubSpace *_evacuateMemorySubSpace; /**< cached pointer to evacuate subspace within active subspace */
	MM_MemorySubSpace *_survivorMemorySubSpace; /**< cached pointer to survivor subspace within active subspace */
	MM_MemorySubSpace *_tenureMemorySubSpace;

	void *_evacuateSpaceBase, *_evacuateSpaceTop;	/**< cached base and top heap pointers within evacuate subspace */
	void *_survivorSpaceBase, *_survivorSpaceTop;	/**< cached base and top heap pointers within survivor subspace */

	uintptr_t _tenureMask; /**< A bit mask indicating which generations should be tenured on scavenge. */
	bool _expandFailed;
	bool _failedTenureThresholdReached;
	uintptr_t _failedTenureLargestObject;
	uintptr_t _countSinceForcingGlobalGC;

	bool _expandTenureOnFailedAllocate;
	bool _cachedSemiSpaceResizableFlag;
	uintptr_t _minTenureFailureSize;
	uintptr_t _minSemiSpaceFailureSize;

	MM_CycleState _cycleState;  /**< Embedded cycle state to be used as the master cycle state for GC activity */
	MM_CollectionStatisticsStandard _collectionStatistics;  /** Common collect stats (memory, time etc.) */

	void *_heapBase;  /**< Cached base pointer of heap */
	void *_heapTop;  /**< Cached top pointer of heap */

public:
	OMR_VM *_omrVM;
	
	/*
	 * Function members
	 */
private:
	/**
	 * Hook callback. Called when a global collect has started
	 */
	static void hookGlobalCollectionStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);

	/**
	 * Hook callback. Called when a global collect has completed
	 */
	static void hookGlobalCollectionComplete(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
	
	/**
	 *  This method is called on the start of a global GC.
	 *  @param env the current thread.
	 */
	void globalCollectionStart(MM_EnvironmentBase *env);
	
	/**
	 *  This method is called on the completion of a global GC.
	 *  @param env the current thread.
	 */
	void globalCollectionComplete(MM_EnvironmentBase *env);

protected:
	/**
 	 * Request for percolate GC
 	 * 
 	 * @return true if Global GC was executed, false if concurrent kickoff forced or Global GC is not possible 
 	 */
	bool percolateGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, PercolateReason percolateReason, uint32_t gcCode);
	
	void reportGCCycleStart(MM_EnvironmentBase *env);
	void reportGCCycleEnd(MM_EnvironmentBase *env);
	void reportGCCycleFinalIncrementEnding(MM_EnvironmentBase *envModron);

	MMINLINE void clearExpandFailedFlag() { _expandFailed = false; };
	MMINLINE void setExpandFailedFlag() { _expandFailed = true; };
	MMINLINE bool expandFailed() { return _expandFailed; };

	MMINLINE void clearFailedTenureThresholdFlag() { _failedTenureThresholdReached = false; };
	MMINLINE void setFailedTenureThresholdFlag() { _failedTenureThresholdReached = true; };
	MMINLINE void setFailedTenureLargestObject(uintptr_t size) { _failedTenureLargestObject = size; };
	MMINLINE uintptr_t getFailedTenureLargestObject() { return _failedTenureLargestObject; };
	MMINLINE bool failedTenureThresholdReached() { return _failedTenureThresholdReached; };

	/**
	 * Determine whether the object pointer is found within the heap proper.
	 * @return Boolean indicating if the object pointer is within the heap boundaries.
	 */
	MMINLINE bool isHeapObject(omrobjectptr_t objectPtr)
	{
		return ((_heapBase <= (uint8_t *)objectPtr) && (_heapTop > (uint8_t *)objectPtr));
	}

	void reportGCStart(MM_EnvironmentBase *env);
	void reportGCEnd(MM_EnvironmentBase *env);
	void reportGCIncrementStart(MM_EnvironmentBase *env);
	void reportGCIncrementEnd(MM_EnvironmentBase *env);
	void reportScavengeStart(MM_EnvironmentBase *env);
	void reportScavengeEnd(MM_EnvironmentBase *env);

	virtual void setupForGC(MM_EnvironmentBase *env);
	virtual void masterSetupForGC(MM_EnvironmentBase *env);
	virtual void workerSetupForGC(MM_EnvironmentBase *env);

	MMINLINE uintptr_t
	isTiltedScavenge()	
	{
		return _extensions->tiltedScavenge ? 1 : 0;
	}

	MMINLINE bool isRememberedSetInOverflowState() { return _extensions->isRememberedSetInOverflowState(); }
	MMINLINE void setRememberedSetOverflowState() { _extensions->setRememberedSetOverflowState(); }
	MMINLINE void clearRememberedSetOverflowState() { _extensions->clearRememberedSetOverflowState(); }

	void clearGCStats(MM_EnvironmentBase *env);
	virtual void mergeGCStats(MM_EnvironmentBase *env);
	virtual bool canCalcGCStats(MM_EnvironmentBase *env);
	void calcGCStats(MM_EnvironmentBase *env);

	virtual void scavenge(MM_EnvironmentBase *env) = 0;
	virtual bool scavengeCompletedSuccessfully(MM_EnvironmentBase *env);
	void masterThreadGarbageCollect(MM_EnvironmentBase *env);
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	virtual bool internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription);
	virtual void internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode);
	virtual void internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace);

	/**
	 * Calculate tilt ratio
	 * @return tiltRatio
	 */
	uintptr_t calculateTiltRatio();

	/**
	 * The implementation of the Lookback scavenger tenure strategy.
	 * This strategy would, for each object age, check the survival history of
	 * that generation of objects (a diagonal down-left check in the survival
	 * history). If, historically, the survival rate of that generation of
	 * objects is always above minimumSurvivalRate, that age will be set for
	 * tenure this scavenge.
	 * @param minimumSurvivalRate The minimum survival rate required to consider tenuring.
	 * @return A tenure mask for the resulting ages to tenure.
	 */
	uintptr_t calculateTenureMaskUsingLookback(double minimumSurvivalRate);

	/**
	 * The implementation of the History scavenger tenure strategy.
	 * This strategy would, for each object age, check the survival history of
	 * that age (a vertical check in the survival history). If, historically,
	 * the survival rate of that age is always above minimumSurvivalRate, that
	 * age will be set for tenure this scavenge.
	 * @param minimumSurvivalRate The minimum survival rate required to consider tenuring.
	 * @return A tenure mask for the resulting ages to tenure.
	 */
	uintptr_t calculateTenureMaskUsingHistory(double minimumSurvivalRate);

	/**
	 * The implementation of the Fixed scavenger tenure strategy.
	 * This strategy will tenure any object who's age is above or equal to
	 * tenureAge.
	 * @param tenureAge The tenure age that objects should be tenured at.
	 * @return A tenure mask for the resulting ages to tenure.
	 */
	uintptr_t calculateTenureMaskUsingFixed(uintptr_t tenureAge);

	/**
	 * Calculates which generations should be tenured in the form of a bit mask.
	 * @return mask of ages to tenure
	 */
	uintptr_t calculateTenureMask();

	/**
	 * process LargeAllocateStats before GC
	 * merge largeObjectAllocateStats in nursery space(no averaging)
	 * @param env Master GC thread.
	 */
	virtual void processLargeAllocateStatsBeforeGC(MM_EnvironmentBase *env);

	/**
	 * process LargeAllocateStats after GC
	 * merge and average largeObjectAllocateStats in tenure space
	 * merge FreeEntry AllocateStats in tenure space
	 * estimate Fragmentation
	 * @param env Master GC thread.
	 */
	virtual void processLargeAllocateStatsAfterGC(MM_EnvironmentBase *env);
	
	/**
	 * reset LargeAllocateStats in Tenure Space
	 * @param env Master GC thread.
	 */
	void resetTenureLargeAllocateStats(MM_EnvironmentBase *env);

public:

	MMINLINE bool
	isObjectInNewSpace(omrobjectptr_t objectPtr)
	{
		return ((void *)objectPtr >= _survivorSpaceBase) && ((void *)objectPtr < _survivorSpaceTop);
	}

	MMINLINE bool
	isObjectInEvacuateMemory(omrobjectptr_t objectPtr)
	{
		/* check if the object in cached allocate (from GC perspective, evacuate) ranges */
		return ((void *)objectPtr >= _evacuateSpaceBase) && ((void *)objectPtr < _evacuateSpaceTop);
	}

	virtual uintptr_t getVMStateID();

	virtual void collectorExpanded(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, uintptr_t expandSize);
	virtual bool canCollectorExpand(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, uintptr_t expandSize);
	virtual uintptr_t getCollectorExpandSize(MM_EnvironmentBase *env);

	virtual void heapReconfigured(MM_EnvironmentBase *env);

	MM_Scavenger(MM_EnvironmentBase *env, MM_CollectorLanguageInterface *cli) :
		MM_Collector(cli),
		_extensions(env->getExtensions()),
		_activeSubSpace(NULL),
		_evacuateMemorySubSpace(NULL),
		_survivorMemorySubSpace(NULL),
		_tenureMemorySubSpace(NULL),
		_evacuateSpaceBase(NULL),
		_evacuateSpaceTop(NULL),
		_survivorSpaceBase(NULL),
		_survivorSpaceTop(NULL),
		_tenureMask(0),
		_expandFailed(false),
		_failedTenureThresholdReached(false),
		_countSinceForcingGlobalGC(0),
		_expandTenureOnFailedAllocate(true),
		_minTenureFailureSize(UDATA_MAX),
		_minSemiSpaceFailureSize(UDATA_MAX),
		_cycleState(),
		_collectionStatistics(),
		_heapBase(NULL),
		_heapTop(NULL),
		_omrVM(env->getOmrVM())
	{
		_typeId = __FUNCTION__;
		_cycleType = OMR_GC_CYCLE_TYPE_SCAVENGE;
	}
};

#endif /* OMR_GC_MODRON_SCAVENGER */
#endif /* SCAVENGER_HPP_ */
