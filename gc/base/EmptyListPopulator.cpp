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

#include "HeapRegionDescriptor.hpp"
#include "ObjectHeapBufferedIterator.hpp"

#include "EmptyListPopulator.hpp"

void 
MM_EmptyListPopulator::initializeObjectHeapBufferedIteratorState(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state) const
{
}

void
MM_EmptyListPopulator::reset(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state, void* base, void* top) const
{
}

uintptr_t 
MM_EmptyListPopulator::populateObjectHeapBufferedIteratorCache(omrobjectptr_t* cache, uintptr_t count, GC_ObjectHeapBufferedIteratorState* state) const
{
	return 0;
}

void
MM_EmptyListPopulator::advance(uintptr_t size, GC_ObjectHeapBufferedIteratorState* state) const
{
}
