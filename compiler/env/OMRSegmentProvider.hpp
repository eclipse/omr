/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef OMR_SEGMENTPROVIDER_INCL
#define OMR_SEGMENTPROVIDER_INCL

#include <set>
#include <deque>
#include "env/SegmentAllocator.hpp"
#include "env/MemorySegment.hpp"

#include "env/TypedAllocator.hpp"
#include "infra/ReferenceWrapper.hpp"
#include "env/RawAllocator.hpp"
#include "env/SegmentProvider.hpp"

namespace OMR {

class SegmentProvider
   {
public:
   SegmentProvider(size_t defaultSegmentSize, size_t allocateSegmentSize, size_t allocationLimit, TR::SegmentAllocator &segmentAllocator, TR::RawAllocator rawAllocator);
   ~SegmentProvider() throw();

   size_t defaultSegmentSize()                     { return _defaultSegmentSize; }
   size_t bytesAllocated()                         { return _bytesAllocated; }
   size_t bytesProvided()                          { return _bytesProvided; }
   size_t allocationLimit()                        { return _allocationLimit; }
   bool isLargeSegment(size_t segmentSize)         { return segmentSize > _segmentAllocateSize; }
   void setAllocationLimit(size_t allocationLimit) { _allocationLimit = allocationLimit; }

   // for compatibility, should  be removed after names changed
   size_t regionBytesAllocated() { return bytesProvided(); }
   size_t systemBytesAllocated() { return bytesAllocated(); }

   virtual TR::MemorySegment &request(size_t requiredSize);
   virtual void release(TR::MemorySegment &segment) throw();

protected:
   size_t round(size_t requestedSize);
   TR::MemorySegment &provideNewSegment(size_t size, TR::reference_wrapper<BackingSegment> systemSegment);
   TR::MemorySegment &createSegmentFromArea(size_t size, void * segmentArea);

   size_t _defaultSegmentSize;
   size_t _segmentAllocateSize;
   size_t _allocationLimit;
   size_t _bytesAllocated;
   size_t _bytesProvided;
   TR::SegmentAllocator & _segmentAllocator;

   typedef TR::typed_allocator<
      TR::reference_wrapper<BackingSegment>,
      TR::RawAllocator
      > SegmentDequeAllocator;

   std::deque<
      TR::reference_wrapper<BackingSegment>,
      SegmentDequeAllocator
      > _allocatedSegments;

   typedef TR::typed_allocator<
      TR::MemorySegment,
      TR::RawAllocator
      > SegmentSetAllocator;

   std::set<
      TR::MemorySegment,
      std::less< TR::MemorySegment >,
      SegmentSetAllocator
      > _providedSegments;

   typedef TR::typed_allocator<
      TR::reference_wrapper<TR::MemorySegment>,
      TR::RawAllocator
      > FreeSegmentDequeAllocator;

   std::deque<
      TR::reference_wrapper<TR::MemorySegment>,
      FreeSegmentDequeAllocator
      > _freeSegments;

   // Current active segment from where memory might be allocated.
   // A segment with space larger than _segmentSize is used for only one request,
   // and is to be released when the release method is invoked, e.g. when a TR::Region
   // goes out of scope. Thus _currentSegment is not allowed to hold such segment.
   TR::reference_wrapper<BackingSegment> _currentSegment;

   };

} // namespace OMR

#endif // OMR_SEGMENTPROVIDER_INCL
