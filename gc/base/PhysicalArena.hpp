/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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
