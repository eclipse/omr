/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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


#ifndef OMREXAMPLEVM_HPP_
#define OMREXAMPLEVM_HPP_

#include "omr.h"
#include "hashtable_api.h"
#include "objectdescription.h"

typedef struct OMR_VM_Example {
	OMR_VM *_omrVM;
	OMR_VMThread *_omrVMThread;
	J9HashTable *rootTable;
	J9HashTable *objectTable;
	omrthread_t self;
	omrthread_rwmutex_t _vmAccessMutex;
	volatile uintptr_t _vmExclusiveAccessCount;
} OMR_VM_Example;

typedef struct RootEntry {
	const char *name;
	omrobjectptr_t rootPtr;
} RootEntry;

typedef struct ObjectEntry {
	const char *name;
	omrobjectptr_t objPtr;
	int32_t numOfRef;
} ObjectEntry;

uintptr_t rootTableHashFn(void *entry, void *userData);
uintptr_t rootTableHashEqualFn(void *leftEntry, void *rightEntry, void *userData);

uintptr_t objectTableHashFn(void *entry, void *userData);
uintptr_t objectTableHashEqualFn(void *leftEntry, void *rightEntry, void *userData);
uintptr_t objectTableFreeFn(void *entry, void *userData);

#endif /* OMREXAMPLEVM_HPP_ */
