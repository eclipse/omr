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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef TR_MEMORY_SEGMENT
#define TR_MEMORY_SEGMENT

#pragma once

#include <functional>
#include <stddef.h>
#include <stdint.h>
#include "env/RawAllocator.hpp"
#include "infra/Assert.hpp"

namespace TR {

class MemorySegment
   {
public:
   typedef void * RawSegment;

   MemorySegment(void * const segment, size_t const size) :
      _segment(segment),
      _size(size),
      _allocated(0),
      _next(this)
      {
      }

   MemorySegment(const MemorySegment &other) :
      _segment(other._segment),
      _size(other._size),
      _allocated(other._allocated),
      _next(this)
      {
      TR_ASSERT(_allocated == 0 && other._next == &other, "Copying segment descriptor that's in use");
      }

   ~MemorySegment() throw() {}

   void * base() const throw()
      {
      return _segment;
      }

   void * allocate(size_t bytes)
      {
      TR_ASSERT( !(_allocated + bytes > _size), "Requested allocation would overflow");
      uint8_t * requested = static_cast<uint8_t *>(_segment) + _allocated;
      _allocated += bytes;
      return requested;
      }

   void reset() throw()
      {
      _allocated = 0;
      }

   size_t remaining() const throw()
      {
      return _size - _allocated;
      }

   size_t size() const throw()
      {
      return _size;
      }

   void link(MemorySegment &next) throw()
      {
      TR_ASSERT(_next == this, "Already linked");
      _next = &next;
      }

   MemorySegment &unlink() throw()
      {
      TR_ASSERT(_next != 0 && _next != this, "Already unlinked");
      MemorySegment &chain = *_next;
      _next = this;
      return chain;
      }

   MemorySegment &next() const throw()
      {
      TR_ASSERT(_next, "_next should never be null");
      return *_next;
      }

   friend bool operator <(const MemorySegment &left, const MemorySegment& right)
      {
      std::less<void *> lessThan;
      return lessThan(left._segment, right._segment);
      }

   friend bool operator ==(const MemorySegment &left, const MemorySegment& right)
      {
      return &left == &right;
      }

   friend bool operator !=(const MemorySegment &left, const MemorySegment& right)
      {
      return !operator ==(left, right);
      }

private:
   void * const _segment;
   size_t const _size;
   size_t _allocated;
   /*
    * As much as it makes me sad, I believe this can't be a
    * reference_wrapper as the class isn't fully defined at this point.
    */
   MemorySegment *_next;
   };

}

#endif // MEMORYSEGMENT
