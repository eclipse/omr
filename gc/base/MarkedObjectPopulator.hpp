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

#if !defined(MARKEDOBJECTPOPULATOR_HPP_)
#define MARKEDOBJECTPOPULATOR_HPP_

#include "ObjectHeapBufferedIteratorPopulator.hpp"

class MM_MarkedObjectPopulator : public MM_ObjectHeapBufferedIteratorPopulator 
{
public:
	MM_MarkedObjectPopulator() : MM_ObjectHeapBufferedIteratorPopulator()
	{
		_typeId = __FUNCTION__;
	}
	
	virtual void initializeObjectHeapBufferedIteratorState(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state) const;
	virtual uintptr_t populateObjectHeapBufferedIteratorCache(omrobjectptr_t* cache, uintptr_t count, GC_ObjectHeapBufferedIteratorState* state) const;
	virtual void advance(uintptr_t size, GC_ObjectHeapBufferedIteratorState* state) const;
	virtual void reset(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state, void* base, void* top) const;
};

#endif /*MARKEDOBJECTPOPULATOR_HPP_*/
