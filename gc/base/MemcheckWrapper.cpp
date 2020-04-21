/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/*
 * Wrapper for communication between valgrind and GC.
*/

#include "omrcfg.h"
#if defined(OMR_VALGRIND_MEMCHECK)

#include "MemcheckWrapper.hpp"

#include "hashtable_api.h"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "HashTableIterator.hpp"

static uintptr_t hashFn(void *key, void *userData);
static uintptr_t hashEqualFn(void *leftKey, void *rightKey, void *userData);

static uintptr_t hashEqualFn(void *leftKey, void *rightKey, void *userData)
{
    return *(uintptr_t *)leftKey == *(uintptr_t *)rightKey;
}

static uintptr_t hashFn(void *key, void *userData)
{
    return *(uintptr_t *)key;
}

void valgrindCreateMempool(MM_GCExtensionsBase *extensions, MM_EnvironmentBase *env, uintptr_t poolAddr)
{
    //1 lets valgrind know that objects will be defined when allocated
    VALGRIND_CREATE_MEMPOOL(poolAddr, 0, 1);
    extensions->valgrindMempoolAddr = poolAddr;

    MUTEX_INIT(extensions->memcheckHashTableMutex);
    MUTEX_ENTER(extensions->memcheckHashTableMutex);
    const char *tableName = "MemcheckWrapper";
    uint32_t entrySize = sizeof(uintptr_t);

    extensions->memcheckHashTable = hashTableNew(env->getPortLibrary(),
                                                 tableName,
                                                 0,
                                                 entrySize,
                                                 0,
                                                 0,
                                                 OMRMEM_CATEGORY_VM,
                                                 hashFn,
                                                 hashEqualFn,
                                                 0,
                                                 0);
    MUTEX_EXIT(extensions->memcheckHashTableMutex);
}

void valgrindDestroyMempool(MM_GCExtensionsBase *extensions)
{
    if (extensions->valgrindMempoolAddr != 0)
    {
        //All objects should have been freed by now!
        VALGRIND_DESTROY_MEMPOOL(extensions->valgrindMempoolAddr);
        MUTEX_ENTER(extensions->memcheckHashTableMutex);
        extensions->valgrindMempoolAddr = 0;
        hashTableFree(extensions->memcheckHashTable);
        extensions->memcheckHashTable = NULL;
        MUTEX_EXIT(extensions->memcheckHashTableMutex);
        MUTEX_DESTROY(extensions->memcheckHashTableMutex);
    }
}

void valgrindMempoolAlloc(MM_GCExtensionsBase *extensions, uintptr_t baseAddress, uintptr_t size)
{
#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF_BACKTRACE("Allocating an object at 0x%lx of size %lu\n", baseAddress, size);
#endif /* defined(VALGRIND_REQUEST_LOGS) */

    /* Allocate object in Valgrind memory pool. */
    VALGRIND_MEMPOOL_ALLOC(extensions->valgrindMempoolAddr, baseAddress, size);
    MUTEX_ENTER(extensions->memcheckHashTableMutex);
    hashTableAdd(extensions->memcheckHashTable, &baseAddress);
    MUTEX_EXIT(extensions->memcheckHashTableMutex);
}

void valgrindMakeMemDefined(uintptr_t address, uintptr_t size)
{
#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF_BACKTRACE("Marking an area as defined at 0x%lx of size %lu\n", address, size);
#endif /* defined(VALGRIND_REQUEST_LOGS) */

    VALGRIND_MAKE_MEM_DEFINED(address, size);
}

void valgrindMakeMemNoaccess(uintptr_t address, uintptr_t size)
{

#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF_BACKTRACE("Marking an area as noaccess at 0x%lx of size %lu\n", address, size);
#endif /* defined(VALGRIND_REQUEST_LOGS) */

    VALGRIND_MAKE_MEM_NOACCESS(address, size);
}

void valgrindMakeMemUndefined(uintptr_t address, uintptr_t size)
{

#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF_BACKTRACE("Marking an area as undefined at 0x%lx of size %lu\n", address, size);
#endif /* defined(VALGRIND_REQUEST_LOGS) */

    VALGRIND_MAKE_MEM_UNDEFINED(address, size);
}

MMINLINE void valgrindFreeObjectDirect(MM_GCExtensionsBase *extensions, uintptr_t baseAddress)
{
    int objSize = (int)((GC_ObjectModel)extensions->objectModel).getConsumedSizeInBytesWithHeader((omrobjectptr_t)baseAddress);

#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF_BACKTRACE("Clearing an object at 0x%lx of size %d\n", baseAddress, objSize);
#endif /* defined(VALGRIND_REQUEST_LOGS) */

    VALGRIND_CHECK_MEM_IS_DEFINED(baseAddress, objSize);
    VALGRIND_MEMPOOL_FREE(extensions->valgrindMempoolAddr, baseAddress);
}

void valgrindClearRange(MM_GCExtensionsBase *extensions, uintptr_t baseAddress, uintptr_t size)
{
    if (size == 0)
    {
        return;
    }
    uintptr_t topInclusiveAddr = baseAddress + size - 1;

#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF_BACKTRACE("Clearing objects in range b/w 0x%lx and  0x%lx\n", baseAddress, topInclusiveAddr);
#endif /* defined(VALGRIND_REQUEST_LOGS) */

    MUTEX_ENTER(extensions->memcheckHashTableMutex);
    GC_HashTableIterator it(extensions->memcheckHashTable);
    uintptr_t *currentSlotPointer = (uintptr_t *)it.nextSlot();
    while (currentSlotPointer != NULL)
    {
        if (baseAddress <= *currentSlotPointer && topInclusiveAddr >= *currentSlotPointer)
        {
            valgrindFreeObjectDirect(extensions, *currentSlotPointer);
            it.removeSlot();
        }
        currentSlotPointer = (uintptr_t *)it.nextSlot();
    }
    MUTEX_EXIT(extensions->memcheckHashTableMutex);

    /* Valgrind automatically marks free objects as noaccess.
    We still mark the entire region as no access for any left out areas */
    valgrindMakeMemNoaccess(baseAddress, size);
}

void valgrindFreeObject(MM_GCExtensionsBase *extensions, uintptr_t baseAddress)
{
    int objSize;
	bool const compressed = extensions->compressObjectReferences();
    if (MM_ForwardedHeader((omrobjectptr_t)baseAddress, compressed).isForwardedPointer())
    {
        /* In scavanger an object may act as pointer to another object(it's replica in another region).
           In this case, getConsumedSizeInBytesWithHeader returns some junk value.
           So instead we calculate the size of the object (replica) it is pointing to 
           and use it for freeing original object.
        */
        omrobjectptr_t fwObject = MM_ForwardedHeader((omrobjectptr_t)baseAddress, compressed).getForwardedObject();
        objSize = (int)((GC_ObjectModel)extensions->objectModel).getConsumedSizeInBytesWithHeader(fwObject);
    }
    else
    {
        objSize = (int)((GC_ObjectModel)extensions->objectModel).getConsumedSizeInBytesWithHeader((omrobjectptr_t)baseAddress);
    }

#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF_BACKTRACE("Clearing an object at 0x%lx of size %d\n", baseAddress, objSize);
#endif /* defined(VALGRIND_REQUEST_LOGS) */

    VALGRIND_CHECK_MEM_IS_DEFINED(baseAddress, objSize);
    VALGRIND_MEMPOOL_FREE(extensions->valgrindMempoolAddr, baseAddress);

    MUTEX_ENTER(extensions->memcheckHashTableMutex);
    hashTableRemove(extensions->memcheckHashTable, &baseAddress);
    MUTEX_EXIT(extensions->memcheckHashTableMutex);
}

bool valgrindCheckObjectInPool(MM_GCExtensionsBase *extensions, uintptr_t baseAddress)
{
#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF("Checking for an object at 0x%lx\n", baseAddress);
#endif /* defined(VALGRIND_REQUEST_LOGS) */

    MUTEX_ENTER(extensions->memcheckHashTableMutex);
    bool exists = hashTableFind(extensions->memcheckHashTable, &baseAddress) != NULL ? true : false;
    MUTEX_EXIT(extensions->memcheckHashTableMutex);
    return exists;
}

void valgrindResizeObject(MM_GCExtensionsBase *extensions, uintptr_t baseAddress, uintptr_t oldSize, uintptr_t newSize)
{

#if defined(VALGRIND_REQUEST_LOGS)
    VALGRIND_PRINTF_BACKTRACE("Resizing an object at 0x%lx from size %d to %d\n", baseAddress, (int)oldSize, (int)newSize);
#endif /* defined(VALGRIND_REQUEST_LOGS) */

    /* We could have used VALGRIND_MEMPOOL_CHANGE request to let Valgrind know of moved object
    but it is very slow without an internal hack. (https://bugs.kde.org/show_bug.cgi?id=366817)*/
    // VALGRIND_CHECK_MEM_IS_DEFINED(baseAddress, oldSize);

    /* Valgrind already knows former size of object allocated at baseAddress. So it will 
    mark the area from baseAddress to oldSize-1 noaccess on a free request as desired*/
    VALGRIND_MEMPOOL_FREE(extensions->valgrindMempoolAddr, baseAddress);

    /* And we don't need to remove and add same address in extensions->_allocatedObjects */
    VALGRIND_MEMPOOL_ALLOC(extensions->valgrindMempoolAddr, baseAddress, newSize);
}

#endif /* defined(OMR_VALGRIND_MEMCHECK) */
