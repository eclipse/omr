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

#ifndef TRLIST_INCL
#define TRLIST_INCL
#undef max
#undef min
#include <list>
#undef round
#include "env/TypedAllocator.hpp"
#include "env/TRMemory.hpp"
namespace TR
   {
   template <class T, class Alloc = TR::Allocator> class list : public std::list<T, TR::typed_allocator<T, Alloc> >
      {
      public:
      list(TR::typed_allocator<T, Alloc> ta) :
         std::list<T, TR::typed_allocator<T, Alloc> > (ta)
         {
         }

#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 4000
      /* A bug in libc++ before 4.0 caused std::list::remove to call the
       * default constructor of TR::typed_allocator, which is not implemented.
       */
      void remove(const T& value)
         {
         this->remove_if([value](const T& value2){return value == value2;});
         }
#endif

      };
   }
#endif
