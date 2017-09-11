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

#include "env/SegmentPool.hpp"
#include "env/MemorySegment.hpp"

TR::SegmentPool::SegmentPool(TR::SegmentProvider &backingProvider, size_t poolSize, TR::RawAllocator rawAllocator) :
   SegmentProvider(backingProvider.defaultSegmentSize()),
   _poolSize(poolSize),
   _storedSegments(0),
   _backingProvider(backingProvider),
   _segmentStack(StackContainer(DequeAllocator(rawAllocator)))
   {
   }

TR::SegmentPool::~SegmentPool() throw()
   {
   while (!_segmentStack.empty())
      {
      TR_ASSERT(_storedSegments > 0, "Too many segments");
      TR::MemorySegment &topSegment = _segmentStack.top().get();
      _segmentStack.pop();
      _backingProvider.release(topSegment);
      --_storedSegments;
      }
   TR_ASSERT(0 == _storedSegments, "Lost a segment");
   }

TR::MemorySegment &
TR::SegmentPool::request(size_t requiredSize)
   {
   if (
      requiredSize <= defaultSegmentSize()
      && !_segmentStack.empty()
      )
      {
      --_storedSegments;
      TR_ASSERT(0 <= _storedSegments, "We lost a segment");
      TR::MemorySegment &recycledSegment = _segmentStack.top().get();
      _segmentStack.pop();
      recycledSegment.reset();
      return recycledSegment;
      }
   return _backingProvider.request(requiredSize);
   }

void
TR::SegmentPool::release(TR::MemorySegment &segment) throw()
   {
   if (
      segment.size() == defaultSegmentSize()
      && _storedSegments < _poolSize
      )
      {
      try
         {
         _segmentStack.push(TR::ref(segment));
         ++_storedSegments;
         }
      catch (...)
         {
         _backingProvider.release(segment);
         }
      }
   else
      {
      _backingProvider.release(segment);
      }
   }
