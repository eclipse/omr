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
 ******************************************************************************/

#if !defined(MARKINGSCHEME_HPP_)
#define MARKINGSCHEME_HPP_

#include "omrcfg.h"
#include "omr.h"

#include "BaseVirtual.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "MarkMap.hpp"
#include "ModronAssertions.h"
#include "ObjectModel.hpp"
#include "WorkStack.hpp"

#define SCAN_MAX (uintptr_t)(-1)
#define BITS_PER_BYTE 8

class MM_CollectorLanguageInterface;

/**
 * @todo Provide class documentation
 */
class MM_MarkingScheme : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:
	OMR_VM *_omrVM;
protected:
	MM_GCExtensionsBase *_extensions;
	MM_CollectorLanguageInterface *_cli;
	MM_MarkMap *_markMap;
	MM_WorkPackets *_workPackets;
	void *_heapBase;
	void *_heapTop;
public:

	/*
	 * Function members
	 */
private:
	MMINLINE void
	assertSaneObjectPtr(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
	{
		Assert_GC_true_with_message(env, objectPtr != J9_INVALID_OBJECT, "Invalid object pointer %p\n", objectPtr);
		Assert_MM_objectAligned(env, objectPtr);
		Assert_GC_true_with_message3(env, isHeapObject(objectPtr), "Object %p not in heap range [%p,%p)\n", objectPtr, _heapBase, _heapTop);
	}

	MM_WorkPackets *createWorkPackets(MM_EnvironmentBase *env);

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	void completeMarking(MM_EnvironmentBase *env);

public:
	static MM_MarkingScheme *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	
	void masterSetupForGC(MM_EnvironmentBase *env);
	void masterSetupForWalk(MM_EnvironmentBase *env);
	void masterCleanupAfterGC(MM_EnvironmentBase *env);
	void workerSetupForGC(MM_EnvironmentBase *env);

	/**
	 *  Initialization for Mark
	 *  Actual startup for Mark procedure
	 *  @param[in] env - passed Environment 
	 *  @param[in] initMarkMap instruct should mark map be initialized too
	 */
	void markLiveObjectsInit(MM_EnvironmentBase *env, bool initMarkMap);

	/**
	 *  Mark Roots
	 *  Create Root Scanner and Mark all roots including classes and classloaders if dynamic class unloading is enabled
	 *  @param[in] env - passed Environment 
	 */
	void markLiveObjectsRoots(MM_EnvironmentBase *env);

	/**
	 *  Scan (complete)
	 *  Process all work packets from queue, including classes and work generated during this phase.
	 *  On return, all live objects will be marked, although some unreachable objects (such as finalizable 
	 *  objects) may still be revived by by subsequent stages.   
	 *  @param[in] env - passed Environment 
	 */
	void markLiveObjectsScan(MM_EnvironmentBase *env);
	
	/**
	 *  Final Mark services including scanning of Clearables
	 *  @param[in] env - passed Environment 
	 */
	void markLiveObjectsComplete(MM_EnvironmentBase *env);


	MMINLINE bool
	inlineMarkObjectNoCheck(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool leafType = false)
	{
		assertSaneObjectPtr(env, objectPtr);

		/* If bit not already set in mark map then set it */
		if (!_markMap->atomicSetBit(objectPtr)) {
			return false;
		}

		/* mark successful - Attempt to add to the work stack */
		if(!leafType) {
			env->_workStack.push(env, (void *)objectPtr);
		}

		env->_markStats._objectsMarked += 1;

		return true;
	}

	MMINLINE bool
	inlineMarkObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool leafType=false)
	{
		bool didMark = false;

		if (NULL != objectPtr) {
			didMark = inlineMarkObjectNoCheck(env, objectPtr, leafType);
		}

		return didMark;
	}

	bool markObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool leafType = false);
	bool markObjectNoCheck(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool leafType = false);

	MMINLINE bool
	isMarked(omrobjectptr_t objectPtr)
	{
		bool shouldCheck = isHeapObject(objectPtr);
		if(shouldCheck) {
			return _markMap->isBitSet(objectPtr);
		}

		/* Everything else is considered marked */
		return true;
	}

	uintptr_t numMarkBitsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop);
	uintptr_t setMarkBitsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop, bool clear);
	uintptr_t numHeapBytesPerMarkMapByte() { return (_markMap->getObjectGrain() * BITS_PER_BYTE); };

	void completeScan(MM_EnvironmentBase *env);
	
	MMINLINE void
	scanObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_CollectorLanguageInterface::MarkingSchemeScanReason reason)
	{
		if(MM_CollectorLanguageInterface::SCAN_REASON_PACKET == reason) {
			env->_markStats._objectsScanned += 1;
		}
		uintptr_t sizeScanned = _cli->markingScheme_scanObject(env, objectPtr, reason);
		/* Due to concurrent marking and packet overflow _bytesScanned may be much larger than the total live set
		 * because objects may be scanned multiple times.
		 */
		env->_markStats._bytesScanned += sizeScanned;
	}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	MMINLINE uintptr_t
	scanObjectWithSize(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_CollectorLanguageInterface::MarkingSchemeScanReason reason, UDATA sizeToDo)
	{
		if(MM_CollectorLanguageInterface::SCAN_REASON_PACKET == reason) {
			env->_markStats._objectsScanned += 1;
		}
		uintptr_t sizeScanned = _cli->markingScheme_scanObjectWithSize(env, objectPtr, reason, sizeToDo);
		/* Due to concurrent marking and packet overflow _bytesScanned may be much larger than the total live set
		 * because objects may be scanned multiple times.
		 */
		env->_markStats._bytesScanned += sizeScanned;
		return sizeScanned;
	}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

	MMINLINE MM_MarkMap *getMarkMap() { return _markMap; }
	MMINLINE void setMarkMap(MM_MarkMap *markMap) { _markMap = markMap; }
	
	bool isMarkedOutline(omrobjectptr_t objectPtr);
	MMINLINE MM_WorkPackets *getWorkPackets() { return _workPackets; }
	
	bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	MMINLINE bool isHeapObject(omrobjectptr_t objectPtr)
	{
		return ((_heapBase <= (uint8_t *)objectPtr) && (_heapTop > (uint8_t *)objectPtr));
	}

	/**
	 * Create a MarkingScheme object.
	 */
	MM_MarkingScheme(MM_EnvironmentBase *env)
		: MM_BaseVirtual()
		, _omrVM(env->getOmrVM())
		, _extensions(env->getExtensions())
		, _cli(_extensions->collectorLanguageInterface)
		, _markMap(NULL)
		, _workPackets(NULL)
		, _heapBase(NULL)
		, _heapTop(NULL)
	{
		_typeId = __FUNCTION__;
	}
	
	/*
	 * Friends
	 */
	friend class MM_MarkingSchemeRootMarker;
	friend class MM_MarkingSchemeRootClearer;
};

/**
 * @todo Provide typedef documentation
 * @ingroup GC_Modron_Standard
 */
typedef struct markSchemeStackIteratorData {
	MM_MarkingScheme *markingScheme;
	MM_EnvironmentBase *env;
} markSchemeStackIteratorData;

#endif /* MARKINGSCHEME_HPP_ */

