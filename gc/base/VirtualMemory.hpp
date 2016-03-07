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

/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(VIRTUALMEMORY_HPP_)
#define VIRTUALMEMORY_HPP_

#include "omrcomp.h"
#include "omrport.h"
#include "modronbase.h"
#include "modronopt.h"

#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"

class MM_GCExtensions;
class MM_GCExtensionsBase;
struct J9PortVmemParams;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_VirtualMemory : public MM_BaseVirtual {
/*
 * Data members
 */
private:
	uintptr_t _pageSize; /**< Page size for this virtual memory object (before reservation requested, after reservation real) */
	uintptr_t _pageFlags; /**< Flags describing the pages used for the virtual memory object */
	uintptr_t _tailPadding; /**< The number of bytes of padding at the end of the virtual memory. This padding will be committed into, but not reported as available */
	void* _heapBase; /**< The lowest usable address in the reserved block, once alignment and padding are taken into account */
	void* _heapTop; /**< One byte past the highest usable address in the reserved block, once alignment and padding are taken into account */
	uintptr_t _reserveSize; /**< The total number of bytes reserved, starting from _baseAddress */
	uintptr_t _mode; /**< requested memory mode (memory flags combination) */
	uintptr_t _consumerCount; /**< number of memory consumers attached to this virtual memory instance */
	J9PortVmemIdentifier _identifier;

protected:
	MM_GCExtensionsBase* _extensions;
	void* _baseAddress; /**< The address returned from port - needed for freeing memory at shutdown */
	uintptr_t _heapAlignment;

public:
/*
 * Function members
 */
private:
	bool freeMemory();

protected:
	/*
	 * use "OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE" for mode
	 */
	static MM_VirtualMemory* newInstance(MM_EnvironmentBase* env, uintptr_t heapAlignment, uintptr_t size, uintptr_t pageSize, uintptr_t pageFlags, uintptr_t tailPadding, void* preferredAddress, void* ceiling, uintptr_t mode, uintptr_t options, uint32_t memoryCategory);

	bool initialize(MM_EnvironmentBase* env, uintptr_t size, void* preferredAddress, void* ceiling, uintptr_t options, uint32_t memoryCategory);
	virtual void tearDown(MM_EnvironmentBase* env);

	virtual void* reserveMemory(J9PortVmemParams* params);

	MM_VirtualMemory(MM_EnvironmentBase* env, uintptr_t heapAlignment, uintptr_t pageSize, uintptr_t pageFlags, uintptr_t tailPadding, uintptr_t mode)
		: MM_BaseVirtual()
		, _pageSize(pageSize)
		, _pageFlags(pageFlags)
		, _tailPadding(tailPadding)
		, _heapBase(0)
		, _heapTop(0)
		, _reserveSize(0)
		, _mode(mode)
		, _consumerCount(0)
		, _identifier()
		, _extensions(env->getExtensions())
		, _baseAddress(NULL)
		, _heapAlignment(heapAlignment)
	{
		_typeId = __FUNCTION__;
	}

	virtual void kill(MM_EnvironmentBase* env);

	virtual bool commitMemory(void* address, uintptr_t size);
	virtual bool decommitMemory(void* address, uintptr_t size, void* lowValidAddress, void* highValidAddress);
	void roundDownTop(uintptr_t rounding);

	/*
	 * Set the NUMA affinity for the specified range within the receiver.
	 * 
	 * @param numaNode - the node to associate the memory with
	 * @param[in] address - the start of the range to modify, must be aligned to the physical page size
	 * @param byteAmount - the size of the range to modify. Might NOT to be aligned to page size. Will be aligned inside.
	 * 
	 * @return true on success, false on failure
	 */
	virtual bool setNumaAffinity(uintptr_t numaNode, void* address, uintptr_t byteAmount);

	/**
	 * Return the heap base of the virtual memory object.
	 */
	MMINLINE void* getHeapBase()
	{
		return _heapBase;
	};

	/**
	 * Return the top of the heap of the virtual memory object.
	 */
	MMINLINE void* getHeapTop()
	{
		return _heapTop;
	};

	/** 
	 * Return the size of the pages used in the virtual memory object
	 */
	MMINLINE uintptr_t getPageSize()
	{
		return _pageSize;
	}

	/** 
	 * Return the flags describing the pages used in the virtual memory object
	 */
	MMINLINE uintptr_t getPageFlags()
	{
		return _pageFlags;
	}

	/**
	 * Return number of memory consumers attached to this virtual memory object
	 * @return consumers number
	 */
	MMINLINE uintptr_t getConsumerCount()
	{
		return _consumerCount;
	}

	/**
	 * Increment number of memory consumers attached to this virtual memory object by one
	 */
	MMINLINE void incrementConsumerCount()
	{
		_consumerCount += 1;
	}

	/**
	 * Decrement number of memory consumers attached to this virtual memory object by one
	 */
	MMINLINE void decrementConsumerCount()
	{
		_consumerCount -= 1;
	}

public:
/*
 * friends
 */
	friend class MM_MemoryManager;
};

#endif /* VIRTUALMEMORY_HPP_ */
