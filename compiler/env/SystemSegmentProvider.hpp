/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef OMR_SYSTEM_SEGMENT_PROVIDER
#define OMR_SYSTEM_SEGMENT_PROVIDER

#pragma once

#ifndef TR_SYSTEM_SEGMENT_PROVIDER
#define TR_SYSTEM_SEGMENT_PROVIDER
namespace OMR { class SystemSegmentProvider; }
namespace TR { using OMR::SystemSegmentProvider; }
#endif

#include <stddef.h>
#include <set>
#include "env/TypedAllocator.hpp"
#include "infra/ReferenceWrapper.hpp"
#include "env/SegmentAllocator.hpp"
#include "env/RawAllocator.hpp"

namespace OMR {

class SystemSegmentProvider : public TR::SegmentAllocator
   {
public:
   SystemSegmentProvider(size_t segmentSize, TR::RawAllocator rawAllocator);
   ~SystemSegmentProvider() throw();
   virtual TR::MemorySegment &request(size_t requiredSize);
   virtual void release(TR::MemorySegment &segment) throw();
   size_t bytesAllocated() const throw();
   size_t regionBytesAllocated() const throw();
   size_t systemBytesAllocated() const throw();
   size_t allocationLimit() const throw();
   void setAllocationLimit(size_t);

private:
   TR::RawAllocator _rawAllocator;
   size_t _currentBytesAllocated;
   size_t _highWaterMark;
   typedef TR::typed_allocator<
      TR::MemorySegment,
      TR::RawAllocator
      > SegmentSetAllocator;

   std::set<
      TR::MemorySegment,
      std::less< TR::MemorySegment >,
      SegmentSetAllocator
      > _segments;

   };

} // namespace TR

#endif // OMR_SYSTEM_SEGMENT_PROVIDER
