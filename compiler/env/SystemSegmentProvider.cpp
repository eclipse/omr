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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "env/SystemSegmentProvider.hpp"
#include "env/MemorySegment.hpp"

OMR::SystemSegmentProvider::SystemSegmentProvider(size_t segmentSize, TR::RawAllocator rawAllocator) :
   TR::SegmentAllocator(segmentSize),
   _rawAllocator(rawAllocator),
   _currentBytesAllocated(0),
   _highWaterMark(0),
   _segments(std::less< TR::MemorySegment >(), SegmentSetAllocator(rawAllocator))
   {
   }

OMR::SystemSegmentProvider::~SystemSegmentProvider() throw()
   {
   }

TR::MemorySegment &
OMR::SystemSegmentProvider::request(size_t requiredSize)
   {
   size_t adjustedSize = ( ( requiredSize + (defaultSegmentSize() - 1) ) / defaultSegmentSize() ) * defaultSegmentSize();
   void *newSegmentArea = _rawAllocator.allocate(adjustedSize);
   try
      {
      auto result = _segments.insert( TR::MemorySegment(newSegmentArea, adjustedSize) );
      TR_ASSERT(result.first != _segments.end(), "Bad iterator");
      TR_ASSERT(result.second, "Insertion failed");
      _currentBytesAllocated += adjustedSize;
      _highWaterMark = _currentBytesAllocated > _highWaterMark ? _currentBytesAllocated : _highWaterMark;
      return const_cast<TR::MemorySegment &>(*(result.first));
      }
   catch (...)
      {
      _rawAllocator.deallocate(newSegmentArea);
      throw;
      }
   }

void
OMR::SystemSegmentProvider::release(TR::MemorySegment &segment) throw()
   {
   auto it = _segments.find(segment);
   _rawAllocator.deallocate(segment.base());
   _currentBytesAllocated -= segment.size();
   TR_ASSERT(it != _segments.end(), "Segment lookup should never fail");
   _segments.erase(it);
   }

size_t
OMR::SystemSegmentProvider::bytesAllocated() const throw()
   {
   return _highWaterMark;
   }

size_t
OMR::SystemSegmentProvider::regionBytesAllocated() const throw()
   {
   return _highWaterMark;
   }

size_t
OMR::SystemSegmentProvider::systemBytesAllocated() const throw()
   {
   return _highWaterMark;
   }

size_t
OMR::SystemSegmentProvider::allocationLimit() const throw()
   {
   return static_cast<size_t>(-1);
   }

void
OMR::SystemSegmentProvider::setAllocationLimit(size_t)
   {
   return;
   }
