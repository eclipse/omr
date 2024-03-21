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
 *
******************************************************************************/

#if !defined(SparseAddressOrderedFixedSizeDataPool_HPP_)
#define SparseAddressOrderedFixedSizeDataPool_HPP_

#include "omrpool.h"
#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "SparseHeapLinkedFreeHeader.hpp"

/**
 *
 * @ingroup GC_Base_Core
 */

class MM_SparseDataTableEntry
{
/*
 * Data members
 */
private:
public:
	void *_dataPtr; /**< Object data pointer related to proxy object */
	void *_proxyObjPtr; /**< Pointer to proxy object that is residing in-heap */
	uintptr_t _size; /**< Total size of the data pointed to by dataPtr */

/*
 *  Function members
 */
private:
public:
	MM_SparseDataTableEntry()
		: _dataPtr(NULL)
		, _proxyObjPtr(NULL)
		, _size(0)
	{
	}

	MM_SparseDataTableEntry(void *dataPtr)
		: _dataPtr(dataPtr)
		, _proxyObjPtr(NULL)
		, _size(0)
	{
	}

	MM_SparseDataTableEntry(void *dataPtr, void* proxyObjPtr, uintptr_t size)
		: _dataPtr(dataPtr)
		, _proxyObjPtr(proxyObjPtr)
		, _size(size)
	{
	}
};

class MM_SparseAddressOrderedFixedSizeDataPool : public MM_BaseVirtual
{
/*
 * Data members
 */
public:
protected:
	uintptr_t _approxLargestFreeEntry;  /**< largest free entry found at the end of a global GC cycle */
	void *_largestFreeEntryAddr; /**< Largest free entry location */
	uintptr_t _approximateFreeMemorySize;  /**< The approximate number of bytes free that could be made part of the free list */
	uintptr_t _lastFreeBytes; /**< Number of bytes free at end of last GC */
	uintptr_t _freeListPoolFreeNodesCount; /**< Number of free list nodes. There's always at least one node in list therefore >= 1 */
	uintptr_t _freeListPoolAllocBytes; /**< Byte amount allocated from sparse heap */

	MM_GCExtensionsBase *_extensions; /**< GC Extensions for this JVM */
	J9Pool *_freeListPool; /**< Memory pool to be used to create MM_SparseHeapLinkedFreeHeader nodes */
	MM_SparseHeapLinkedFreeHeader *_heapFreeList; /**< List of free node regions available at sparse heap */

private:
	J9HashTable *_objectToSparseDataTable; /**< Map from object addresses to its corresponsing data address at sparse heap */

/*
 * Function members
 */
public:

	static MM_SparseAddressOrderedFixedSizeDataPool *newInstance(MM_EnvironmentBase *env, void *sparseHeapBase, uintptr_t sparseDataPoolSize);
	virtual void kill(MM_EnvironmentBase *env);

	/**
	 * Finds first available free region that fits parameter size
	 *
	 * @param: region size
	 * @return address of free region or NULL if there's no such contiguous free region
	 */

	void *findFreeListEntry(uintptr_t size);

	/**
	 * A region was freed, now we insert that back into the freeList ordered by address
	 *
	 * @param address	void*		Address associated to region to be returned
	 * @param size		uintptr_t	Size of region to be returned to freeList
	 */
	bool returnFreeListEntry(void *address, uintptr_t size);

	/**
	 * Add object entry to the hash table that maps the proxyObjPtr to the data pointer
	 *
	 * @param dataPtr		void*		data location pointer
	 * @param proxyObjPtr	void*		Proxy object associated with dataPtr
	 * @param size			uintptr_t	Size of region consumed by dataPtr
	 *
	 * @return true if object is added successfully to the hash table , false otherwise
	 */
	bool mapSparseDataPtrToHeapProxyObjectPtr(void *dataPtr, void *proxyObjPtr, uintptr_t size);

	/**
	 * Remove entry from the hash table that is associated the object data pointer provided
	 *
	 * @param dataPtr	void*	Data pointer
	 *
	 * @return true if key associated to dataPtr is removed successfully, false otherwise
	 */
	bool unmapSparseDataPtrFromHeapProxyObjectPtr(void *dataPtr);

	/**
	 * Get MM_SparseDataTableEntry associated with data pointer
	 *
	 * @param dataPtr	void*	Data pointer
	 * @return in-heap proxy object pointer of data pointer
	 */
	MM_SparseDataTableEntry *findSparseDataTableEntryForSparseDataPtr(void *dataPtr);
	/**
	 * Get data size in bytes associated with the data pointer
	 *
	 * @param dataPtr	void*	Data pointer
	 * @return size of data pointer in bytes
	 */
	uintptr_t findObjectDataSizeForSparseDataPtr(void *dataPtr);

	/**
	 * Get proxy object pointer associated with data pointer
	 *
	 * @param dataPtr	void*	Data pointer
	 * @return in-heap proxy object pointer of data pointer
	 */
	void *findHeapProxyObjectPtrForSparseDataPtr(void *dataPtr);

	/**
	 * Check if the given data pointer is valid
	 *
	 * @param dataPtr	void*	Data pointer
	 * @return true if data pointer is valid
	 */
	bool isValidDataPtr(void *dataPtr);

	/**
	 * Get the largest free list entry
	 */
	MMINLINE uintptr_t getLargestFreeEntry()
	{
		return _approxLargestFreeEntry;
	}

	/**
	 * Set the largest free list entry
	 */
	MMINLINE void setLargestFreeEntry(uintptr_t approxLargestFreeEntry)
	{
		_approxLargestFreeEntry = approxLargestFreeEntry;
	}

	/**
	 * Get the total free list pool allocated bytes
	 */
	MMINLINE uintptr_t getFreeListPoolAllocBytes()
	{
		return _freeListPoolAllocBytes;
	}

	/**
	 * Update the proxyObjPtr after an object has moved for the sparse data entry associated with the given dataPtr.
	 *
	 * @param dataPtr		void*	Data pointer
	 * @param proxyObjPtr	void*	Updated in-heap proxy object pointer for data pointer
	 *
	 * @return true if the sparse data entry was successfully updated, false otherwise
	 */
	bool updateSparseDataEntryAfterObjectHasMoved(void *dataPtr, void *proxyObjPtr);

protected:
	bool initialize(MM_EnvironmentBase *env, void *sparseHeapBase);
	void tearDown(MM_EnvironmentBase *env);

	MM_SparseAddressOrderedFixedSizeDataPool(MM_EnvironmentBase *env, uintptr_t sparseDataPoolSize)
		: MM_BaseVirtual()
		, _approxLargestFreeEntry(sparseDataPoolSize)
		, _largestFreeEntryAddr(NULL)
		, _approximateFreeMemorySize(sparseDataPoolSize)
		, _lastFreeBytes(0)
		, _freeListPoolFreeNodesCount(0)
		, _freeListPoolAllocBytes(0)
		, _extensions(env->getExtensions())
		, _freeListPool(NULL)
		, _heapFreeList(NULL)
		, _objectToSparseDataTable(NULL)
	{
		_typeId = __FUNCTION__;
	}

private:
	/**
	 * Update a sparse heap free list node.
	 *
	 * @param node		MM_SparseHeapLinkedFreeHeader free list node to update
	 * @param address	Updated address field value for the given  MM_SparseHeapLinkedFreeHeader node
	 * @param size		Updated size field value for the given  MM_SparseHeapLinkedFreeHeader node
	 * @param next		Updated next field value for the given  MM_SparseHeapLinkedFreeHeader node
	 *
	 * @return the new MM_SparseHeapLinkedFreeHeader if successfully created, null otherwise
	 */
	void updateSparseHeapFreeListNode(MM_SparseHeapLinkedFreeHeader *node, void *address, uintptr_t size, MM_SparseHeapLinkedFreeHeader *next);

	/**
	 * Create a new sparse heap free list node.
	 *
	 * @param dataPtr		void*		Data pointer initialization value for the the new sparse heap free list node
	 * @param size			uintptr_t	Size initialization value for the the new sparse heap free list node
	 *
	 * @return the new MM_SparseHeapLinkedFreeHeader if successfully created, null otherwise
	 */
	MM_SparseHeapLinkedFreeHeader *createNewSparseHeapFreeListNode(void *dataAddr, uintptr_t size);

	/**
	 * Frees all nodes from free List
	 */
	void freeAllSparseHeapFreeListNodes();

	/**
	 * J9HashTableHashFn provided for _objectToSparseDataTable hashtable creation
	 */
	static uintptr_t entryHash(void *entry, void *userData);

	/**
	 * J9HashTableEqualFn provided for _objectToSparseDataTable hashtable creation
	 */
	static uintptr_t entryEquals(void *leftEntry, void *rightEntry, void *userData);
};

#endif /* SparseAddressOrderedFixedSizeDataPool_HPP_ */
