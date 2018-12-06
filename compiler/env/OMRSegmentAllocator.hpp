/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef OMR_SEGMENTALLOCATOR_INCL
#define OMR_SEGMENTALLOCATOR_INCL

#pragma once

#include "env/RawAllocator.hpp"
#include "env/MemorySegment.hpp"

namespace OMR { class SegmentAllocator; }

namespace OMR
{

struct BackingSegment
   {
   BackingSegment(void *b, size_t s) { _base=b; _size=s; }
   void   *_base;
   size_t  _size;

   void  *base() { return _base; }
   size_t size() { return _size; }
   };


// Simplest possible "segment" allocator just wraps raw memory allocations in a BackingSegment
class SegmentAllocator : public RawAllocator
   {
public:
   SegmentAllocator() : RawAllocator()
      {
      }

   SegmentAllocator(const SegmentAllocator & other) : RawAllocator(other)
      {
      }

   bool matchesSegment(BackingSegment & memory, TR::MemorySegment & segment)
      {
      return memory.base() == segment.base();
      }

   BackingSegment &allocate(size_t size, void * hint = 0)
      {
      void *p = malloc(sizeof(BackingSegment) + size);
      if (!p)
         throw std::bad_alloc();
      BackingSegment *b = reinterpret_cast<BackingSegment *>(p);
      new (b) BackingSegment(b+1, size);
      return *b;
      }

   void deallocate(BackingSegment & p) throw()
      {
      free(&p);
      }

   void deallocate(BackingSegment & p, const size_t size) throw()
      {
      deallocate(p);
      }

   // make the given BackingSegment inaccessible, typically used to debug memory allocations
   void protect(BackingSegment & p) throw()
      {
      }

   // default implementation is raw allocation of precise size, so cannot allocate more
   ptrdiff_t remaining(const BackingSegment &memorySegment) { return 0; };

   };
}

void *operator new(size_t size, OMR::BackingSegment &segment) throw();
void *operator new[](size_t size, OMR::BackingSegment &segment) throw();

void operator delete(void *, OMR::BackingSegment &segment) throw();
void operator delete[](void *ptr, OMR::BackingSegment  &segment) throw();

#endif // OMR_SEGMENTALLOCATOR_INCL
