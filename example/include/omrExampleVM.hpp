/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
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
