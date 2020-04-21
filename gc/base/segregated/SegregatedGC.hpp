/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#if !defined(SEGREGATEDGC_HPP_)
#define SEGREGATEDGC_HPP_

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"

#include "CollectionStatisticsStandard.hpp"
#include "GlobalCollector.hpp"
#include "MarkMap.hpp"
#include "SegregatedMarkingScheme.hpp"
#include "SweepSchemeSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_SegregatedGC : public MM_GlobalCollector
{
	/*
	 * Data members
	 */
protected:
	MM_GCExtensionsBase *_extensions;
	OMRPortLibrary *_portLibrary;
	MM_SegregatedMarkingScheme *_markingScheme;
	MM_SweepSchemeSegregated *_sweepScheme;
	MM_Dispatcher *_dispatcher;

	MM_CycleState _cycleState;  /**< Embedded cycle state to be used as the master cycle state for GC activity */
	MM_CollectionStatisticsStandard _collectionStatistics; /** Common collect stats (memory, time etc.) */
private:
public:
	/* OMRTODO Remove _objectsMarked and _scanBytes, they are used to fake marking to create more interesting verbose output */
	uintptr_t _scanBytes;
	uintptr_t _objectsMarked;

	/*
	 * Function members
	 */
private:
protected:
	void reportGCIncrementStart(MM_EnvironmentBase *env);
	void reportGCIncrementEnd(MM_EnvironmentBase *env);
	void reportGCCycleStart(MM_EnvironmentBase *env);
	void reportGCCycleEnd(MM_EnvironmentBase *env);
	void reportGCCycleFinalIncrementEnding(MM_EnvironmentBase *env);

	void reportGCStart(MM_EnvironmentBase *env);
	void reportGCEnd(MM_EnvironmentBase *env);

	void reportMarkStart(MM_EnvironmentBase *env);
	void reportMarkEnd(MM_EnvironmentBase *env);
	void reportSweepStart(MM_EnvironmentBase *env);
	void reportSweepEnd(MM_EnvironmentBase *env);

public:
	static MM_SegregatedGC *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	virtual bool collectorStartup(MM_GCExtensionsBase* extensions);
	virtual void collectorShutdown(MM_GCExtensionsBase* extensions);

	virtual void setupForGC(MM_EnvironmentBase*);
	virtual void abortCollection(MM_EnvironmentBase* env, CollectionAbortReason reason);

	virtual void* createSweepPoolState(MM_EnvironmentBase* env, MM_MemoryPool* memoryPool);
	virtual void deleteSweepPoolState(MM_EnvironmentBase* env, void* sweepPoolState);

	virtual bool internalGarbageCollect(MM_EnvironmentBase*, MM_MemorySubSpace*, MM_AllocateDescription*);
	virtual void internalPreCollect(MM_EnvironmentBase*, MM_MemorySubSpace*, MM_AllocateDescription*, uint32_t);
	virtual void internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace);

	virtual uintptr_t getVMStateID() { return 100; }

	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace,uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);

	virtual bool isMarked(void *objectPtr) { return _markingScheme->isMarked(static_cast<omrobjectptr_t>(objectPtr)); }

	/**
	 * Return reference to Marking Scheme
	 */
	MM_SegregatedMarkingScheme *getMarkingScheme()
	{
		return _markingScheme;
	}
	
	MM_SweepSchemeSegregated *getSweepScheme()
	{
		return _sweepScheme;
	}

	MM_SegregatedGC(MM_EnvironmentBase *env)
		: MM_GlobalCollector()
		, _extensions(MM_GCExtensionsBase::getExtensions(env->getOmrVM()))
		, _portLibrary(env->getPortLibrary())
		, _markingScheme(NULL)
		, _sweepScheme(NULL)
		, _dispatcher(_extensions->dispatcher)
		, _scanBytes(0)
		, _objectsMarked(0)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* SEGREGATEDGC_HPP_ */
