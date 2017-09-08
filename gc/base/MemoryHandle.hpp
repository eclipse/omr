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


#if !defined(MEMORYHANDLE_HPP)
#define MEMORYHANDLE_HPP

class MM_VirtualMemory;

class MM_MemoryHandle {
	/*
	 * Data members
	 */
private:
	MM_VirtualMemory* _virtualMemory; /**< pointer to virtual memory instance */
	void* _memoryBase; /**< lowest memory address */
	void* _memoryTop; /**< highest memory address */

protected:
public:

	/*
	 * Function members
	 */
private:
protected:
	/**
	 * Set _virtualMemory field
	 * @param virtualMemory virtual memory pointer to set
	 */
	MMINLINE void setVirtualMemory(MM_VirtualMemory* virtualMemory)
	{
		_virtualMemory = virtualMemory;
	}

	/**
	 * Return back virtual memory pointer
	 * @return virtual memory pointer
	 */
	MMINLINE MM_VirtualMemory* getVirtualMemory() const
	{
		return _virtualMemory;
	}

	/**
	 * Set memory base pointer
	 * @param memoryBase memory base pointer
	 */
	MMINLINE void setMemoryBase(void* memoryBase)
	{
		_memoryBase = memoryBase;
	}

	/**
	 * Return back memory base pointer
	 * @return memory base pointer
	 */
	MMINLINE void* getMemoryBase()
	{
		return _memoryBase;
	}

	/**
	 * Set memory top pointer
	 * @param memoryTop memory top pointer
	 */
	MMINLINE void setMemoryTop(void* memoryTop)
	{
		_memoryTop = memoryTop;
	}

	/**
	 * Return back memory top pointer
	 * @return memory top pointer
	 */
	MMINLINE void* getMemoryTop()
	{
		return _memoryTop;
	}

public:
	MM_MemoryHandle()
		: _virtualMemory(NULL)
		, _memoryBase(NULL)
		, _memoryTop(NULL) {};

	/*
	 * friends
	 */
	friend class MM_MemoryManager;
};

#endif /* MEMORYHANDLE_HPP */
