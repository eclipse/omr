/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef NODEEXTENSION_INCL
#define NODEEXTENSION_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"

#define NUM_DEFAULT_ELEMS 1
#define NUM_DEFAULT_NODECHILDREN 2

typedef TR_ArenaAllocator TR_NodeExtAllocator;

namespace TR
{

/**
 * A flexible data storage class for associating information with nodes.
 *
 * \note This class works via overallocation, which I would prefer to see
 *       go away in lieu of a more dynamically sized object that is less
 *       discomforting.
 */
class NodeExtension
    {
public:
    NodeExtension(TR_NodeExtAllocator & alloc) :
       _allocator(alloc) {};

    void * operator new (size_t s, uint16_t arrayElems, TR_NodeExtAllocator & m)
       {
       s += (arrayElems - NUM_DEFAULT_ELEMS) * sizeof(uintptr_t);
       return m.allocate(s);
       }

    void   operator delete(void * ptr, uint16_t arrayElems, TR_NodeExtAllocator & m)
       {
       size_t size = sizeof(NodeExtension) + (arrayElems - NUM_DEFAULT_ELEMS) * sizeof(uintptr_t);
       return m.deallocate(ptr, size);
       }

    void   operator delete(void * ptr, size_t s, TR_NodeExtAllocator & m)
       {
       m.deallocate(ptr,s);
       };

    template <class T>
    T  setElem(uint16_t index, T elem)
       {
       return (T)(_data[index] = (uintptr_t)elem);
       }

    template <class T>
    T  getElem(uint16_t index)
       {
       return (T)_data[index];
       }

    void
    freeExtension(size_t s)
       {
       _allocator.deallocate(this,s);
       }

  private:
      TR_NodeExtAllocator & _allocator;
      uintptr_t _data[NUM_DEFAULT_ELEMS];
    };

}

#endif
