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

#ifndef OMR_PERSISTENT_ALLOCATOR
#define OMR_PERSISTENT_ALLOCATOR

#pragma once

#ifndef TR_PERSISTENT_ALLOCATOR
#define TR_PERSISTENT_ALLOCATOR

namespace OMR { class PersistentAllocator; }
namespace TR { using OMR::PersistentAllocator; }

#endif // TR_PERSISTENT_ALLOCATOR

#include <stddef.h>
#include <stdint.h>

#include "env/RawAllocator.hpp"
#include "env/PersistentAllocatorKit.hpp"

namespace OMR {

class PersistentAllocator
   {
public:
   PersistentAllocator(const TR::PersistentAllocatorKit &allocatorKit);

   void *allocate(size_t size, const std::nothrow_t tag, void * hint = 0) throw();
   void * allocate(size_t size, void * hint = 0);
   void deallocate(void * p, const size_t sizeHint = 0) throw();

   friend bool operator ==(const PersistentAllocator &left, const PersistentAllocator &right)
      {
      return left._rawAllocator == right._rawAllocator;
      }

   friend bool operator !=(const PersistentAllocator &left, const PersistentAllocator &right)
      {
      return !operator ==(left, right);
      }

private:
   PersistentAllocator(const PersistentAllocator &);

   TR::RawAllocator _rawAllocator;

   };

}

inline void * operator new(size_t size, OMR::PersistentAllocator &allocator)
   {
   return allocator.allocate(size);
   }

inline void * operator new(size_t size, OMR::PersistentAllocator &allocator, const std::nothrow_t& tag) throw()
   {
   return allocator.allocate(size, tag);
   }

inline void * operator new[](size_t size, OMR::PersistentAllocator &allocator)
   {
   return operator new(size, allocator);
   }

inline void * operator new[](size_t size, OMR::PersistentAllocator &allocator, const std::nothrow_t& tag) throw()
   {
   return operator new(size, allocator, tag);
   }

inline void operator delete(void *ptr, OMR::PersistentAllocator &allocator) throw()
   {
   allocator.deallocate(ptr);
   }

inline void operator delete(void *ptr, OMR::PersistentAllocator &allocator, const std::nothrow_t& tag) throw()
   {
   allocator.deallocate(ptr);
   }

inline void operator delete[](void *ptr, OMR::PersistentAllocator &allocator) throw()
   {
   operator delete(ptr, allocator);
   }

inline void operator delete[](void *ptr, OMR::PersistentAllocator &allocator, const std::nothrow_t& tag) throw()
   {
   operator delete(ptr, allocator, tag);
   }

#endif // OMR_PERSISTENT_ALLOCATOR
