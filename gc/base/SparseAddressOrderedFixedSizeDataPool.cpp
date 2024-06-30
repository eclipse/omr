/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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

#include "omrcomp.h"
#include "modronbase.h"
#include "ModronAssertions.h"
#include "SparseVirtualMemory.hpp"
#include "SparseAddressOrderedFixedSizeDataPool.hpp"
#include "Heap.hpp"

#define SPARSE_DATA_TABLE_INIT_SIZE 10
#define SPARSE_FREE_LIST_POOL_INIT_SIZE 5

MM_SparseAddressOrderedFixedSizeDataPool *
MM_SparseAddressOrderedFixedSizeDataPool::newInstance(MM_EnvironmentBase *env, void *sparseHeapBase, uintptr_t sparseDataPoolSize)
{
	MM_SparseAddressOrderedFixedSizeDataPool *sparseDataPool;

	sparseDataPool = (MM_SparseAddressOrderedFixedSizeDataPool *)env->getForge()->allocate(sizeof(MM_SparseAddressOrderedFixedSizeDataPool), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (sparseDataPool) {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_allocation_success(sparseHeapBase, (void *)sparseDataPoolSize);
		sparseDataPool = new(sparseDataPool) MM_SparseAddressOrderedFixedSizeDataPool(env, sparseDataPoolSize);
		if (!sparseDataPool->initialize(env, sparseHeapBase)) {
			sparseDataPool->kill(env);
			sparseDataPool = NULL;
		}
	} else {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_allocation_failure(sparseHeapBase, sparseDataPoolSize);
	}

	return sparseDataPool;
}

uintptr_t
MM_SparseAddressOrderedFixedSizeDataPool::entryHash(void *entry, void *userData)
{
	return (uintptr_t)((MM_SparseDataTableEntry *)entry)->_dataPtr;
}

uintptr_t
MM_SparseAddressOrderedFixedSizeDataPool::entryEquals(void *leftEntry, void *rightEntry, void *userData)
{
	uintptr_t lhs = (uintptr_t)((MM_SparseDataTableEntry *)leftEntry)->_dataPtr;
	uintptr_t rhs = (uintptr_t)((MM_SparseDataTableEntry *)rightEntry)->_dataPtr;
	return (lhs == rhs);
}

bool
MM_SparseAddressOrderedFixedSizeDataPool::initialize(MM_EnvironmentBase *env, void *sparseHeapBase)
{
	bool ret = true;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	/* Initialize _freeListPool J9Pool */
	_freeListPool = pool_new(sizeof(MM_SparseHeapLinkedFreeHeader), SPARSE_FREE_LIST_POOL_INIT_SIZE, 0, 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_MM, POOL_FOR_PORT(OMRPORTLIB));

	_objectToSparseDataTable = hashTableNew(
		OMRPORTLIB, OMR_GET_CALLSITE(),
		SPARSE_DATA_TABLE_INIT_SIZE,
		sizeof(MM_SparseDataTableEntry),
		sizeof(uintptr_t),
		0,
		OMRMEM_CATEGORY_MM,
		entryHash,
		entryEquals,
		NULL,
		NULL);

	/* Initialize _heapFreeList with initial values */
	_heapFreeList = createNewSparseHeapFreeListNode(sparseHeapBase, _approxLargestFreeEntry);

	if ((NULL == _freeListPool) || (NULL == _objectToSparseDataTable) || (NULL == _heapFreeList)) {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_initialization_failure(sparseHeapBase, _freeListPool, _objectToSparseDataTable, _heapFreeList);
		ret = false;
	} else {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_initialization_success(sparseHeapBase, _freeListPool, _objectToSparseDataTable, _heapFreeList);
	}

	return ret;
}

void
MM_SparseAddressOrderedFixedSizeDataPool::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _freeListPool) {
		pool_kill(_freeListPool);
		_freeListPool = NULL;
	}

	if (NULL != _heapFreeList) {
		freeAllSparseHeapFreeListNodes();
		_heapFreeList = NULL;
	}

	if (NULL != _objectToSparseDataTable) {
		hashTableFree(_objectToSparseDataTable);
		_objectToSparseDataTable = NULL;
	}
}

/**
 * Teardown and free the receiver by invoking the virtual #tearDown() function
 */
void
MM_SparseAddressOrderedFixedSizeDataPool::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_SparseAddressOrderedFixedSizeDataPool::mapSparseDataPtrToHeapProxyObjectPtr(void *dataPtr, void *proxyObjPtr, uintptr_t size)
{
	bool ret = true;
	MM_SparseDataTableEntry entry = MM_SparseDataTableEntry(dataPtr, proxyObjPtr, size);
	void *result = hashTableAdd(_objectToSparseDataTable, &entry);

	if (NULL == result) {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_insertEntry_failure(dataPtr, (void *)size, proxyObjPtr);
		ret = false;
	} else {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_insertEntry_success(dataPtr, (void *)size, proxyObjPtr);
	}

	return ret;
}

bool
MM_SparseAddressOrderedFixedSizeDataPool::unmapSparseDataPtrFromHeapProxyObjectPtr(void *dataPtr)
{
	bool ret = true;
	MM_SparseDataTableEntry entryToRemove = MM_SparseDataTableEntry(dataPtr);

	if (0 != hashTableRemove(_objectToSparseDataTable, &entryToRemove)) {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_removeEntry_failure(dataPtr);
		ret = false;
	} else {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_removeEntry_success(dataPtr);
	}

	return ret;
}

MM_SparseDataTableEntry *
MM_SparseAddressOrderedFixedSizeDataPool::findSparseDataTableEntryForSparseDataPtr(void *dataPtr) {
	MM_SparseDataTableEntry lookupEntry = MM_SparseDataTableEntry(dataPtr);
	return (MM_SparseDataTableEntry *)hashTableFind(_objectToSparseDataTable, &lookupEntry);
}

uintptr_t
MM_SparseAddressOrderedFixedSizeDataPool::findObjectDataSizeForSparseDataPtr(void *dataPtr)
{
	uintptr_t size = 0;
	MM_SparseDataTableEntry *entry = findSparseDataTableEntryForSparseDataPtr(dataPtr);

	if ((NULL == entry) || (entry->_dataPtr != dataPtr)) {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_findEntry_failure(dataPtr);
	} else {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_findEntry_success(dataPtr);
		size = entry->_size;
	}

	return size;
}

void *
MM_SparseAddressOrderedFixedSizeDataPool::findHeapProxyObjectPtrForSparseDataPtr(void *dataPtr)
{
	void *proxyObjPtr = NULL;
	MM_SparseDataTableEntry *entry = findSparseDataTableEntryForSparseDataPtr(dataPtr);

	if ((NULL == entry) || (entry->_dataPtr != dataPtr)) {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_findEntry_failure(dataPtr);
	} else {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_findEntry_success(dataPtr);
		proxyObjPtr = entry->_proxyObjPtr;
	}

	return proxyObjPtr;
}

bool
MM_SparseAddressOrderedFixedSizeDataPool::isValidDataPtr(void *dataPtr)
{
	MM_SparseDataTableEntry *entry = findSparseDataTableEntryForSparseDataPtr(dataPtr);
	bool ret = true;

	if (entry != NULL) {
		ret = (entry->_dataPtr == dataPtr);
	}

	return ret;
}

MM_SparseHeapLinkedFreeHeader *
MM_SparseAddressOrderedFixedSizeDataPool::createNewSparseHeapFreeListNode(void *dataAddr, uintptr_t size)
{
	MM_SparseHeapLinkedFreeHeader *newFreeListNode = (MM_SparseHeapLinkedFreeHeader *)pool_newElement(_freeListPool);
	if (NULL != newFreeListNode) {
		newFreeListNode->setAddress(dataAddr);
		newFreeListNode->setSize(size);
		newFreeListNode->_next = NULL;
		_freeListPoolFreeNodesCount++;
	}

	return newFreeListNode;
}

void *
MM_SparseAddressOrderedFixedSizeDataPool::findFreeListEntry(uintptr_t size)
{
	Assert_MM_true(_freeListPoolFreeNodesCount > 0);
	MM_SparseHeapLinkedFreeHeader *previous = NULL;
	MM_SparseHeapLinkedFreeHeader *current = _heapFreeList;
	void *returnAddr = NULL;
	uintptr_t currSize = 0;

	/* Find big enough free space */
	while (NULL != current) {
		if (current->_size >= size) {
			break;
		}
		previous = current;
		current = current->_next;
	}

	/* Should be impossible for current to be NULL since sparse heap will always have available space, however this case should still be checked */
	if (NULL != current) {
		currSize = current->_size;
		returnAddr = current->_address;
		if (currSize == size) {
			/* Remove current node from FreeList since we're using all of it */
			if (NULL == previous) {
				_heapFreeList = current->_next;
			} else {
				previous->_next = current->_next;
			}
			pool_removeElement(_freeListPool, current);
			_freeListPoolFreeNodesCount--;
		} else {
			/* Update current entry */
			current->setAddress((void *)((uintptr_t)returnAddr + size));
			current->contractSize(size);
			/* Update largest free entry */
			if (_largestFreeEntryAddr == returnAddr) {
				_approxLargestFreeEntry -= size;
				_largestFreeEntryAddr = current->_address;
			}
		}

		Assert_MM_true(NULL != returnAddr);
		_approximateFreeMemorySize -= size;
		_freeListPoolAllocBytes += size;

		Trc_MM_SparseAddressOrderedFixedSizeDataPool_freeListEntryFoundForData_success(returnAddr, (void *)size, _freeListPoolFreeNodesCount, (void *)_approximateFreeMemorySize, (void *)_freeListPoolAllocBytes);
	}

	return returnAddr;
}

bool
MM_SparseAddressOrderedFixedSizeDataPool::returnFreeListEntry(void *dataAddr, uintptr_t size)
{
	MM_SparseHeapLinkedFreeHeader *previous = NULL;
	MM_SparseHeapLinkedFreeHeader *current = _heapFreeList;
	void *endAddress = (void *)((uintptr_t)dataAddr + size);

	/* First find where we should include the new node if needed */
	while (NULL != current) {
		/* Lazily update largest free entry */
		if (current->_size > _approxLargestFreeEntry) {
			_approxLargestFreeEntry = current->_size;
			_largestFreeEntryAddr = current->_address;
		}
		if (dataAddr < current->_address) {
			break;
		}
		previous = current;
		current = current->_next;
	}

	/* Find where we should insert the new space.
	 * The list is constructed in such a way that there will always be at least one node in it.
	 *
	 *** Handle cases where we insert between previous and current ***
	 * Case 1 -> previous -> SPACE -> <here> -> SPACE -> current
	 * Case 2 -> previous -> <here> -> SPACE -> current
	 * Case 3 -> previous -> SPACE -> <here> -> current
	 * Case 4 -> previous -> <here> -> current
	 *
	 *** Search reached end of freeList; therefore, current is NULL, insert after previous ***
	 * Case 5 -> previous -> <here> -> NULL
	 * Case 6 -> previous -> SPACE -> <here> -> NULL
	 *
	 *** previous is NULL; therefore insert before current and make <here> head of freeList ***
	 * Case 7 -> <here> -> SPACE -> current
	 * Case 8 -> <here> -> current
	 */

	/* Case 7 and 8: previous is NULL */
	if (NULL == previous) {
		void *currentAddr = current->_address;
		if (endAddress == currentAddr) {
			current->expandSize(size);
			current->setAddress(dataAddr);
		} else {
			MM_SparseHeapLinkedFreeHeader *newNode = createNewSparseHeapFreeListNode(dataAddr, size);
			newNode->_next = current;
			_heapFreeList = newNode;
		}
        } else {
		/* Should we merge with previous */
		void *previousHighAddr = (void *)((uintptr_t)previous->_address + previous->_size);
		/* Case 2 and 5 */
		if (previousHighAddr == dataAddr) {
			previous->expandSize(size);
			/* Case 4: where node is right between previous and current, therefore merge everything */
			if ((NULL != current) && (current->_address == endAddress)) {
				previous->expandSize(current->_size);
				previous->_next = current->_next;
				pool_removeElement(_freeListPool, current);
				_freeListPoolFreeNodesCount--;
			}
		/* Case 3: Merge node to current only */
		} else if ((NULL != current) && (current->_address == endAddress)) {
			current->expandSize(size);
			current->setAddress(dataAddr);
		/* Cases 1 and 6: Insert new node between nodes or at the end of the list */
		} else {
			Assert_MM_true(previousHighAddr < dataAddr);
			Assert_MM_true((NULL == current) || (current->_address > endAddress));
			MM_SparseHeapLinkedFreeHeader *newNode = createNewSparseHeapFreeListNode(dataAddr, size);
			previous->_next = newNode;
			newNode->_next = current;
		}
	}

	_approximateFreeMemorySize += size;
	_freeListPoolAllocBytes -= size;
	_lastFreeBytes = size;

	Trc_MM_SparseAddressOrderedFixedSizeDataPool_returnFreeListEntry_success(dataAddr, (void *)size, _freeListPoolFreeNodesCount, (void *)_approximateFreeMemorySize, (void *)_freeListPoolAllocBytes);
	return true;
}

bool
MM_SparseAddressOrderedFixedSizeDataPool::updateSparseDataEntryAfterObjectHasMoved(void *dataPtr, void *proxyObjPtr)
{
	bool ret = true;
	MM_SparseDataTableEntry lookupEntry = MM_SparseDataTableEntry(dataPtr);
	MM_SparseDataTableEntry *entry = (MM_SparseDataTableEntry *)hashTableFind(_objectToSparseDataTable, &lookupEntry);

	if ((NULL != entry) && (entry->_dataPtr == dataPtr)) {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_updateEntry_success(dataPtr, entry->_proxyObjPtr, proxyObjPtr);
		entry->_proxyObjPtr = proxyObjPtr;
	} else {
		Trc_MM_SparseAddressOrderedFixedSizeDataPool_findEntry_failure(dataPtr);
		ret = false;
	}

	return ret;
}

void
MM_SparseAddressOrderedFixedSizeDataPool::updateSparseHeapFreeListNode(MM_SparseHeapLinkedFreeHeader *node, void *address, uintptr_t size, MM_SparseHeapLinkedFreeHeader *next)
{
	node->setAddress(address);
	node->setSize(size);
	node->_next = next;
}
void
MM_SparseAddressOrderedFixedSizeDataPool::freeAllSparseHeapFreeListNodes()
{
	MM_SparseHeapLinkedFreeHeader *current = _heapFreeList;

	while (NULL != current) {
		MM_SparseHeapLinkedFreeHeader *temp = current;
		current = current->_next;
		pool_removeElement(_freeListPool, temp);
	}
}
