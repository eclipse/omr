/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

#ifndef DEQUE_HPP
#define DEQUE_HPP

#pragma once

#include <deque>
#include "env/TypedAllocator.hpp"
#include "env/TRMemory.hpp"

namespace TR {

template <typename T, class Alloc = TR::Allocator>
class deque : public std::deque<T, typed_allocator<T, Alloc> >
   {
public:
   typedef typed_allocator<T, Alloc> allocator_type;
   typedef std::deque<T, typed_allocator<T, Alloc> > container_type;
   typedef typename allocator_type::value_type value_type;
   typedef typename allocator_type::reference reference;
   typedef typename allocator_type::const_reference const_reference;
   /*
    * This would ideally use the parent deque's size_type.  However, such usage
    * runs into two-phase lookup problems when compiling with MSVC++ 2010.
    */
   typedef std::size_t size_type;

   explicit deque(const allocator_type &allocator);
   explicit deque(size_type size, const allocator_type &allocator);
   explicit deque(size_type size, const T& initialValue, const allocator_type &allocator);
   template <typename InputIterator> deque(InputIterator first, InputIterator last, const allocator_type &allocator);
   deque(const deque<T, Alloc> &x);
   ~deque();

   reference operator[](size_type index);
   const_reference operator[](size_type index) const;
   };

}

template <typename T, class Alloc>
TR::deque<T, Alloc>::deque(const allocator_type &allocator) :
   container_type(allocator)
   {
   }

template <typename T, class Alloc>
TR::deque<T, Alloc>::deque(size_type size, const allocator_type &allocator) :
   container_type(size, T(), allocator)
   {
   }

template <typename T, class Alloc>
TR::deque<T, Alloc>::deque(size_type size, const T& initialValue, const allocator_type &allocator) :
   container_type(size, initialValue, allocator)
   {
   }

template <typename T, class Alloc>
template <typename InputIterator>
TR::deque<T, Alloc>::deque(InputIterator first, InputIterator last, const allocator_type &allocator) :
   container_type(first, last, allocator)
   {
   }

template <typename T, class Alloc>
TR::deque<T, Alloc>::deque(const deque<T, Alloc> &other) :
   container_type(other)
   {
   }

template <typename T, class Alloc>
TR::deque<T, Alloc>::~deque()
   {
   }

template <typename T, class Alloc>
typename TR::deque<T, Alloc>::reference
TR::deque<T, Alloc>::operator [](size_type index)
   {
// In DEBUG, at() is used for correctness due to its bound checking
// whilst [] is used in PROD for performance
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   return this->at(index);
#else
   return container_type::operator[](index);
#endif // defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   }

template <typename T, class Alloc>
typename TR::deque<T, Alloc>::const_reference
TR::deque<T, Alloc>::operator [](size_type index) const
   {
// In DEBUG, at() is used for correctness due to its bound checking
// whilst [] is used in PROD for performance
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   return this->at(index);
#else
   return container_type::operator[](index);
#endif // defined(DEBUG) || defined(PROD_WITH_ASSUMES)  
   }

#endif // DEQUE_HPP
