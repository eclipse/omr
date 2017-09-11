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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(NONSHAREDVIRTUALMEMORY_HPP_)
#define NONSHAREDVIRTUALMEMORY_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrport.h"
#include "modronopt.h"

#include "BaseVirtual.hpp"
#include "VirtualMemory.hpp"

class MM_EnvironmentBase;

/**
 * Provides facility for management of Virtual memory not associated with the heap.
 * Some methods are implemented for AIX platforms to allow non-heap structures to
 * be malloced.
 * @ingroup GC_Base_Core
 */
class MM_NonVirtualMemory : public MM_VirtualMemory {
/*
 * Data members
 */
private:
protected:
public:
/*
 * Function members
 */
private:
protected:
	static MM_NonVirtualMemory* newInstance(MM_EnvironmentBase* env, uintptr_t heapAlignment, uintptr_t size, uint32_t memoryCategory);

	MM_NonVirtualMemory(MM_EnvironmentBase* env, uintptr_t heapAlignment)
		: MM_VirtualMemory(env, heapAlignment, 1, OMRPORT_VMEM_PAGE_FLAG_NOT_USED, 0, (OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE))
	{
		_typeId = __FUNCTION__;
	};
#if (defined(AIXPPC) && (!defined(PPC64) || defined(OMR_GC_REALTIME))) || defined(J9ZOS39064) || defined(OMRZTPF)
	virtual void tearDown(MM_EnvironmentBase* env);
	virtual void* reserveMemory(J9PortVmemParams* params);
#endif /* (defined(AIXPPC) && (!defined(PPC64) || defined(OMR_GC_REALTIME))) || defined(J9ZOS39064) || defined(OMRZTPF) */
public:
#if (defined(AIXPPC) && (!defined(PPC64) || defined(OMR_GC_REALTIME))) || defined(J9ZOS39064) || defined(OMRZTPF)
	virtual bool commitMemory(void* address, uintptr_t size);
	virtual bool decommitMemory(void* address, uintptr_t size, void* lowValidAddress, void* highValidAddress);
	virtual bool setNumaAffinity(uintptr_t numaNode, void* address, uintptr_t byteAmount);
#endif /* (defined(AIXPPC) && (!defined(PPC64) || defined(OMR_GC_REALTIME))) || defined(J9ZOS39064) || defined(OMRZTPF) */

public:
/*
 * friends
 */
	friend class MM_MemoryManager;
};

#endif /* NONSHAREDVIRTUALMEMORY_HPP_ */
