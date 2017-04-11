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
 ******************************************************************************/

#if !defined(ALLOCATEINITIALIZATION_HPP_)
#define ALLOCATEINITIALIZATION_HPP_

#include "ModronAssertions.h"
#include "objectdescription.h"
#include "omrgcconsts.h"
#include "omrmodroncore.h"
#include "omrutil.h"

#include "AllocateDescription.hpp"
#include "AtomicOperations.hpp"
#include "Base.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ObjectModel.hpp"

/**
 * Base class for language-specific object allocation and initialization. Subclasses
 * must complete initialization of the attached MM_AllocateDescription before calling
 * allocateAndInitializeObject().
 *
 * Allocated memory is zeroed by default; this behavior can be inhibited by selecting
 * the allocate_no_gc allocation flag.
 *
 * If the first allocation attempt fails a garbage collection cycle will be started and
 * the allocation will be retried when the cycle completes; garbage collection on
 * allocation failure can be inhibited by selecting the allocate_no_gc flag.
 *
 * If a generational garbage collector is enabled, new allocations are made from nursery
 * space; select the allocate_tenured flag to force allocation directly into old space.
 * The allocate_tenured is ignored if no generational collector is enabled.
 *
 * For allocation of indexable objects the allocate_indexable flag must be selected.
 *
 * It is expected that the client language will subclass this to represent more
 * complex categories of objects. This base class includes an allocation category
 * field that can be used in the language object model to determine subclass type
 * allocation category and cast the received allocation initialization instance
 * to the appropriate subclass to complete object initialization for the category.
 */
class MM_AllocateInitialization : public MM_Base
{
/*
 * Member data and types
 */
private:
/* Subclasses should only define language specific properties
 * required to initialize an allocated object.
 */

protected:
	const uintptr_t _allocationCategory;		/**< language-defined object category used in GC_ObjectModel::initializeAllocation() */
	const uintptr_t _requestedSizeInBytes;		/**< minimum number of bytes to allocate for object header+instance data */
	bool _isAllocatable;						/**< this is set if the allocation should not proceed */

	MM_AllocateDescription _allocateDescription;/**< mutable allocation descriptor holds actual allocation terms */

public:
	/**
	 * Definitions for flag bits used in object allocation flags passed to constructor.
	 *
	 * @see selectObjectAllocationFlags(bool indexable, bool tenured, bool noZeroMemory, bool noGc)
	 */
	enum ObjectAllocationFlags {
		allocate_tenured = OMR_GC_ALLOCATE_OBJECT_TENURED				/**< select this flag to allocate in old space (gencon only) */
		, allocate_no_zero_memory = OMR_GC_ALLOCATE_OBJECT_NON_ZERO_TLH	/**< select this flag to inhibit zeroing of allocated heap memory */
		, allocate_no_gc = OMR_GC_ALLOCATE_OBJECT_NO_GC					/**< select this flag to inhibit collector cycle start on allocation failure */
		, allocate_indexable = OMR_GC_ALLOCATE_OBJECT_INDEXABLE			/**< select this flag to allocate an indexable object */
	};

/*
 * Member functions
 */
private:
	MMINLINE bool
	shouldZeroMemory(MM_EnvironmentBase *env)
	{
		/* wipe allocated space if requested (NON_ZERO_TLH flag set is a request to not clear) */
		bool shouldZero = !_allocateDescription.getNonZeroTLHFlag();
#if defined(OMR_GC_BATCH_CLEAR_TLH)
		/* Even if clearing is requested, we may decide not to so if the allocation comes from a batch cleared TLH */
		shouldZero &= !(_allocateDescription.isCompletedFromTlh() && env->getExtensions()->batchClearTLH);
#endif /* OMR_GC_BATCH_CLEAR_TLH */
		return shouldZero;
	}

protected:

public:
	MMINLINE static uintptr_t
	selectObjectAllocationFlags(bool indexable, bool tenured, bool noZeroMemory, bool noGc)
	{
		uintptr_t selectedFlags = 0;

		if (tenured) {
			selectedFlags |= (uintptr_t)allocate_tenured;
		}
		if (noZeroMemory) {
			selectedFlags |= (uintptr_t)allocate_no_zero_memory;
		}
		if (noGc) {
			selectedFlags |= (uintptr_t)allocate_no_gc;
		}
		if (indexable) {
			selectedFlags |= (uintptr_t)allocate_indexable;
		}

		return selectedFlags;
	}

	MMINLINE void setAllocatable(bool isAllocatable) { _isAllocatable = isAllocatable; }
	MMINLINE bool isAllocatable() { return _isAllocatable; }

	MMINLINE uintptr_t getAllocationCategory() { return _allocationCategory; }
	MMINLINE uintptr_t getRequestedSizeInBytes() { return _requestedSizeInBytes; }
	MMINLINE MM_AllocateDescription *getAllocateDescription() { return &_allocateDescription; }

	MMINLINE bool
	isGCAllowed()
	{
		return 0 == (OMR_GC_ALLOCATE_OBJECT_NO_GC & _allocateDescription.getAllocateFlags());
	}

	MMINLINE bool
	isIndexable()
	{
		return 0 != (OMR_GC_ALLOCATE_OBJECT_INDEXABLE & _allocateDescription.getAllocateFlags());
	}


	/**
	 * Object allocator and initializer. Will return NULL if !isAllocatable() or
	 * heap allocation fails or allocated object initialization fails. In the
	 * last case, the allocated heap memory will become floating garbage to be
	 * recovered in a subsequent GC.
	 *
	 * This method allocates a single chuck of contiguous memory of the requested
	 * size (adjust up for object alignment if required) and sets the object flags
	 * in the object header slot before calling GC_ObjectModel::initializeAllocation()
	 * to complete the object initialization.
	 *
	 * @param[in] omrVMThread the calling thread
	 * @return Pointer to the initialized object, or NULL
	 *
	 */
	MMINLINE omrobjectptr_t
	allocateAndInitializeObject(OMR_VMThread *omrVMThread)
	{
		MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
		GC_ObjectModel *objectModel = &(env->getExtensions()->objectModel);

		uintptr_t vmState = env->pushVMstate(J9VMSTATE_GC_ALLOCATE_OBJECT);

		Assert_MM_true(_allocateDescription.shouldCollectAndClimb() == isGCAllowed());

		if (isAllocatable()) {
			/* _isAllocatable reset in initializer is carried over from the now-defunct "NoGC"
			 * code path and may be unused - more investigation required. If cached allocations
			 * are enabled, it is assumed that instrumentable allocations are off (otherwise, a
			 * JIT resolve frame is required to report the allocation event).
			 */
			setAllocatable(isGCAllowed() || env->_objectAllocationInterface->cachedAllocationsEnabled(env));
		}

		omrobjectptr_t objectPtr = NULL;
		if (isAllocatable()) {
			void *heapBytes = NULL;
			
			_allocateDescription.setBytesRequested(objectModel->adjustSizeInBytes(_allocateDescription.getBytesRequested()));
			if (isIndexable()) {
				heapBytes = env->_objectAllocationInterface->allocateArrayletSpine(env,
						&_allocateDescription, _allocateDescription.getMemorySpace(), isGCAllowed());
			} else {
				heapBytes = env->_objectAllocationInterface->allocateObject(env,
						&_allocateDescription, _allocateDescription.getMemorySpace(), isGCAllowed());
			}
			_allocateDescription.setAllocationSucceeded(NULL != heapBytes);

			if (NULL != heapBytes) {
				/* wipe allocated space if requested and allowed (NON_ZERO_TLH flag set inhibits zeroing) */
				if (shouldZeroMemory(env)) {
					OMRZeroMemory(heapBytes, _allocateDescription.getContiguousBytes());
				}

				/* set the OMR flags in the object header to match those set by memory space in allocate description */
				objectModel->setObjectFlags((omrobjectptr_t)heapBytes, OMR_OBJECT_METADATA_FLAGS_MASK, _allocateDescription.getObjectFlags());
				/* let the language object model perform its header and object initialization */
				objectPtr = objectModel->initializeAllocation(env, heapBytes, this);

				/* if object model initialization fails heapBytes will become floating garbage to be recovered in next GC */
				if (NULL != objectPtr) {
					/* in case the object escapes to another thread... */
					MM_AtomicOperations::writeBarrier();
					/* reflect the current OMR flags in the object header back into allocate description */
					_allocateDescription.setObjectFlags((uint32_t)objectModel->getObjectFlags(objectPtr));
#if defined(OMR_GC_ALLOCATION_TAX)
					/* if concurrent mark is enabled thread might have to pay tax - must save/restore allocated object in case of GC */
					env->saveObjects(objectPtr);
					_allocateDescription.payAllocationTax(env);
					env->restoreObjects(&objectPtr);
#endif /* OMR_GC_ALLOCATION_TAX */
				}
			}
		}

		if (isGCAllowed()) {
			/* issue Allocation Failure Report if required */
			env->allocationFailureEndReportIfRequired(&_allocateDescription);
			/* Done allocation - successful or not */
			env->unwindExclusiveVMAccessForGC();
		}
		env->popVMstate(vmState);

		return objectPtr;
	}

	/**
	 * Constructor. Properties set here are used to preset defaults in the
	 * MM_AllocationDescription instance that will be passed to the allocation
	 * context. Caller can use defaults if appropriate or modify the properties
	 * in the allocation description (see initializeAllocationDescription())
	 * before calling allocateAndInitializeObject().
	 *
	 * The required size in bytes will be adjusted for object alignment. If not
	 * known, specify 0 and set final adjusted value for allocation description
	 * in an override for initializeAllocationDescription().
	 *
	 * @param[in] env pointer to environment for calling thread
	 * @param[in] allocationCategory language-defined allocation category
	 * @param[in] requiredSizeInBytes minimum (header+data) size in bytes required to be allocated
	 * @param[in] objectAllocationFlags union of ObjectAllocationFlags bits
	 *
	 * @see getAllocateDescription()
	 * @see initializeAllocationDescription(MM_EnvironmentBase *)
	 */
	MM_AllocateInitialization(MM_EnvironmentBase *env,
			uintptr_t allocationCategory,
			uintptr_t requiredSizeInBytes,
			uintptr_t objectAllocationFlags = 0
	) : MM_Base()
		, _allocationCategory(allocationCategory)
		, _requestedSizeInBytes(requiredSizeInBytes)
		, _isAllocatable(true)
		, _allocateDescription(_requestedSizeInBytes, objectAllocationFlags,
				0 == (OMR_GC_ALLOCATE_OBJECT_NO_GC & objectAllocationFlags),
				0 == (OMR_GC_ALLOCATE_OBJECT_NO_GC & objectAllocationFlags))
	{
		if (_allocateDescription.getTenuredFlag()) {
			_allocateDescription.setMemorySpace(env->getExtensions()->heap->getDefaultMemorySpace());
		} else {
			_allocateDescription.setMemorySpace(env->getMemorySpace());
		}
	}
};

#endif /* ALLOCATEINITIALIZATION_HPP_ */
