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

#ifndef OMR_SEGMENT_ALLOCATOR_HPP
#define OMR_SEGMENT_ALLOCATOR_HPP

#pragma once

#include "env/SegmentProvider.hpp"

namespace TR {

class SegmentAllocator : public SegmentProvider
   {
public:
   virtual size_t regionBytesAllocated() const throw() = 0;
   virtual size_t systemBytesAllocated() const throw() = 0;
   virtual size_t allocationLimit() const throw() = 0;
   virtual void setAllocationLimit(size_t) = 0;

protected:
   explicit SegmentAllocator(size_t defaultSegmentSize) :
      SegmentProvider(defaultSegmentSize)
      {
      }

   SegmentAllocator(const SegmentAllocator &other) :
      SegmentProvider(other)
      {
      }

   virtual ~SegmentAllocator() throw();
   };

}

#endif // OMR_SEGMENT_ALLOCATOR_HPP
