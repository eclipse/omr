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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
