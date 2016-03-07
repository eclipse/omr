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


#if !defined(PHYSICALARENA_HPP_)
#define PHYSICALARENA_HPP_

#include "omrcomp.h"
#include "modronbase.h"

#include "BaseVirtual.hpp"

class MM_EnvironmentBase;
class MM_Heap;
class MM_MemorySpace;
class MM_PhysicalSubArena;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_PhysicalArena : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:
	MM_PhysicalArena *_nextArena; /**< In multi-PA environments, points to the next arena in the Heap */
	MM_PhysicalArena *_previousArena; /**< In multi-PA environments, points to the previous arena in the Heap */
	void *_lowAddress; /**< The base address of this Physical Arena */
	void *_highAddress; /**< The byte after the highest address byte addressable within this Physical Arena */
	
protected:
	MM_MemorySpace *_memorySpace;
	MM_Heap *_heap;
	bool _attached;

public:

	/*
	 * Function members
	 */
private:
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

public:
	virtual void kill(MM_EnvironmentBase *env);

	virtual bool inflate(MM_EnvironmentBase *env) = 0;
	virtual bool attachSubArena(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, uintptr_t size, uintptr_t attachPolicy) = 0;
	virtual void detachSubArena(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena) = 0;

	virtual bool canExpand(MM_EnvironmentBase *env, MM_PhysicalSubArena *expandArena);

	MMINLINE MM_MemorySpace *getMemorySpace() { return _memorySpace; }
	MMINLINE void setMemorySpace(MM_MemorySpace *memorySpace) { _memorySpace = memorySpace; }

	MMINLINE void setAttached(bool attached) { _attached = attached; }

	MM_PhysicalArena(MM_EnvironmentBase *env, MM_Heap *heap) :
		_nextArena(NULL),
		_previousArena(NULL),
		_lowAddress(NULL),
		_highAddress(NULL),
		_memorySpace(NULL),
		_heap(heap),
		_attached(false)
	{
		_typeId = __FUNCTION__;
	}

	MM_PhysicalArena(MM_EnvironmentBase *env, MM_MemorySpace *memorySpace) :
		_nextArena(NULL),
		_previousArena(NULL),
		_lowAddress(NULL),
		_highAddress(NULL),
		_memorySpace(memorySpace),
		_heap(NULL),
		_attached(false)
	{
		_typeId = __FUNCTION__;
	}

	MMINLINE MM_PhysicalArena *getNextArena() { return _nextArena; }
	MMINLINE void setNextArena(MM_PhysicalArena *arena) { _nextArena = arena; }
	MMINLINE MM_PhysicalArena *getPreviousArena() { return _previousArena; }
	MMINLINE void setPreviousArena(MM_PhysicalArena *arena) { _previousArena = arena; }

	/* these were promoted from MM_PhysicalArenaVirtualMemory */
	MMINLINE void *getLowAddress() { return _lowAddress; }
	MMINLINE void setLowAddress(void *addr) { _lowAddress = addr; }
	MMINLINE void *getHighAddress() { return _highAddress; }
	MMINLINE void setHighAddress(void *addr) { _highAddress = addr; }
};

#endif /* PHYSICALARENA_HPP_ */
