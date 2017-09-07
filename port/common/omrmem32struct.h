/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef omrmem32struct_h
#define omrmem32struct_h

#include "omrport.h"

typedef struct J9HeapWrapper {
	struct J9HeapWrapper *nextHeapWrapper;
	J9Heap *heap;
	uintptr_t heapSize;
	J9PortVmemIdentifier *vmemID;
} J9HeapWrapper;

typedef struct J9SubAllocateHeapMem32 {
	uintptr_t totalSize;
	J9HeapWrapper *firstHeapWrapper;
	omrthread_monitor_t monitor;
	uintptr_t subCommitCommittedMemorySize;
	BOOLEAN canSubCommitHeapGrow;
	J9HeapWrapper *subCommitHeapWrapper;
	uintptr_t suballocator_initialSize;
	uintptr_t suballocator_commitSize;
} J9SubAllocateHeapMem32;

#endif	/* omrmem32struct_h */
