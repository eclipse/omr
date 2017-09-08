/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

/*
 * Wrapper for communication between valgrind and GC.
*/

#include "omrcfg.h"
#if defined(OMR_VALGRIND_MEMCHECK)

#include "MemcheckWrapper.hpp"
#include "GCExtensionsBase.hpp"

#if defined(OMR_VALGRIND_MEMCHECK)
#include <valgrind/memcheck.h>
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

void valgrindCreateMempool(MM_GCExtensionsBase *extensions,uintptr_t poolAddr)
{
    //1 lets valgrind know that objects will be defined when allocated
    VALGRIND_CREATE_MEMPOOL(poolAddr, 0, 1);
    extensions->valgrindMempoolAddr = poolAddr;
}

void valgrindDestroyMempool(MM_GCExtensionsBase *extensions)
{
    if(extensions->valgrindMempoolAddr != 0)
	{
        //All objects should have been freed by now!
        Assert_MM_true(extensions->_allocatedObjects.empty());
        VALGRIND_DESTROY_MEMPOOL(extensions->valgrindMempoolAddr);
        extensions->valgrindMempoolAddr = 0;
    }
}

void valgrindMempoolAlloc(MM_GCExtensionsBase *extensions, uintptr_t baseAddress, uintptr_t size)
{
#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF_BACKTRACE("Allocated object at 0x%lx of size %lu\n", baseAddress, size);
#endif /* defined(VALGRIND_REQUEST_LOGS) */
    /* Allocate object in Valgrind memory pool. */ 		
    VALGRIND_MEMPOOL_ALLOC(extensions->valgrindMempoolAddr, baseAddress, size);
    extensions->_allocatedObjects.insert(baseAddress);
}

void valgrindMakeMemDefined(uintptr_t address, uintptr_t size)
{
#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF_BACKTRACE("Marking area defined at 0x%lx of size %lu\n", address, size);
#endif /* defined(VALGRIND_REQUEST_LOGS) */
    VALGRIND_MAKE_MEM_DEFINED(address, size);
}

void valgrindMakeMemNoaccess(uintptr_t address, uintptr_t size)
{
#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF_BACKTRACE("Marking area noaccess at 0x%lx of size %lu\n", address, size);
#endif /* defined(VALGRIND_REQUEST_LOGS) */
    VALGRIND_MAKE_MEM_NOACCESS(address, size);
}

void valgrindClearRange(MM_GCExtensionsBase *extensions, uintptr_t baseAddress, uintptr_t size)
{
    if(size == 0)
        return;

    uintptr_t topInclusiveAddr = baseAddress + size - 1;
#if defined(VALGRIND_REQUEST_LOGS)	
    VALGRIND_PRINTF_BACKTRACE("Clearing objects in range b/w 0x%lx and  0x%lx\n", baseAddress,topInclusiveAddr);
#endif /* defined(VALGRIND_REQUEST_LOGS) */

    if(!extensions->_allocatedObjects.empty())
    {	
        std::set<uintptr_t>::iterator it;
        std::set<uintptr_t>::iterator setEnd = extensions->_allocatedObjects.end();
        std::set<uintptr_t>::iterator lBound = extensions->_allocatedObjects.lower_bound(baseAddress);
        std::set<uintptr_t>::iterator uBound = --extensions->_allocatedObjects.end();	

        while(uBound !=  extensions->_allocatedObjects.begin() && *uBound > topInclusiveAddr)
            uBound--;

#if defined(VALGRIND_REQUEST_LOGS)	
        VALGRIND_PRINTF("lBound = %lx, uBound = %lx\n",*lBound,*uBound);			
#endif /* defined(VALGRIND_REQUEST_LOGS) */			
        
        if(*uBound >topInclusiveAddr)
            return; //all elements left in set are greater than our range

        for(it = lBound;*it <= *uBound && it != setEnd;it++)
        {
#if defined(VALGRIND_REQUEST_LOGS)				
            int objSize = (int) ( (GC_ObjectModel)extensions->objectModel ).getConsumedSizeInBytesWithHeader( (omrobjectptr_t) *it);
            VALGRIND_PRINTF("Clearing object at 0x%lx of size %d\n", *it,objSize);
#endif /* defined(VALGRIND_REQUEST_LOGS) */				
            VALGRIND_MEMPOOL_FREE(extensions->valgrindMempoolAddr,*it);
        }
        if(*uBound >= *lBound && uBound != setEnd)
        {
#if defined(VALGRIND_REQUEST_LOGS)				
            VALGRIND_PRINTF("Erasing set from lBound = %lx to uBound = %lx\n",*lBound,*uBound);		
#endif /* defined(VALGRIND_REQUEST_LOGS) */			
            extensions->_allocatedObjects.erase(lBound,++uBound);//uBound is exclusive
        }
    }
}

void valgrindFreeObject(MM_GCExtensionsBase *extensions, uintptr_t baseAddress)
{
#if defined(VALGRIND_REQUEST_LOGS)				    
    int objSize = (int) ( (GC_ObjectModel)extensions->objectModel ).getConsumedSizeInBytesWithHeader( (omrobjectptr_t) baseAddress);
    VALGRIND_PRINTF_BACKTRACE("Clearing object at 0x%lx of size %d\n",baseAddress,objSize);
#endif /* defined(VALGRIND_REQUEST_LOGS) */			    
    VALGRIND_MEMPOOL_FREE(extensions->valgrindMempoolAddr,baseAddress);	
    extensions->_allocatedObjects.erase(baseAddress);
}

bool valgrindCheckObjectInPool(MM_GCExtensionsBase *extensions, uintptr_t baseAddress)
{
    return (extensions->_allocatedObjects.find(baseAddress) != extensions->_allocatedObjects.end());
}

#endif /* defined(OMR_VALGRIND_MEMCHECK) */
