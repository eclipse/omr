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
