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

#ifndef OMR_RAW_ALLOCATOR_HPP
#define OMR_RAW_ALLOCATOR_HPP

#pragma once

#ifndef TR_RAW_ALLOCATOR
#define TR_RAW_ALLOCATOR
namespace OMR { class RawAllocator; }
namespace TR { typedef OMR::RawAllocator RawAllocator; }
#endif

#include <stddef.h>
#include <cstdlib>
#include <new>
#include "env/TypedAllocator.hpp"

namespace OMR {

class RawAllocator
   {
public:
   RawAllocator()
      {
      }

   RawAllocator(const RawAllocator &other)
      {
      }

   void *allocate(size_t size, const std::nothrow_t tag, void * hint = 0) throw()
      {
      return malloc(size);
      }

   void * allocate(size_t size, void * hint = 0)
      {
      void * const alloc = allocate(size, std::nothrow, hint);
      if (!alloc) throw std::bad_alloc();
      return alloc;
      }

   void deallocate(void * p) throw()
      {
      free(p);
      }

   void deallocate(void * p, const size_t size) throw()
      {
      free(p);
      }

   friend bool operator ==(const RawAllocator &left, const RawAllocator &right)
      {
      return true;
      }

   friend bool operator !=(const RawAllocator &left, const RawAllocator &right)
      {
      return !operator ==(left, right);
      }

   template <typename T>
   operator TR::typed_allocator< T, RawAllocator >() throw()
      {
      return TR::typed_allocator< T, RawAllocator >(*this);
      }

   };

}

inline void * operator new(size_t size, OMR::RawAllocator allocator)
   {
   return allocator.allocate(size);
   }

inline void operator delete(void *ptr, OMR::RawAllocator allocator) throw()
   {
   allocator.deallocate(ptr);
   }

inline void * operator new[](size_t size, OMR::RawAllocator allocator)
   {
   return allocator.allocate(size);
   }

inline void operator delete[](void *ptr, OMR::RawAllocator allocator) throw()
   {
   allocator.deallocate(ptr);
   }

inline void * operator new(size_t size, OMR::RawAllocator allocator, const std::nothrow_t& tag) throw()
   {
   return allocator.allocate(size, tag);
   }

inline void * operator new[](size_t size, OMR::RawAllocator allocator, const std::nothrow_t& tag) throw()
   {
   return allocator.allocate(size, tag);
   }

#endif // OMR_RAW_ALLOCATOR_HPP
