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

#include "env/SegmentProvider.hpp"
#include "env/MemorySegment.hpp"
#include <algorithm>

// The SegmentProvider *allocates* segments using the SegmentAllocator (and allocates some of its own memory via RawAllocator)
// and *provides* memory to its clients. In providing TR::MemorySegments to the rest of the compiler allocation classes,
// SegmentProvider tracks how much memory it allocates vis the SegmentAllocator as well as how much memory it has provided
// in the form of TR::MemorySegments.

OMR::SegmentProvider::SegmentProvider(size_t defaultSegmentSize,
                                      size_t segmentAllocateSize,
                                      size_t allocationLimit,
                                      TR::SegmentAllocator &segmentAllocator,
                                      TR::RawAllocator rawAllocator) :
   _defaultSegmentSize(defaultSegmentSize),
   _allocationLimit(allocationLimit),
   _bytesAllocated(0),
   _bytesProvided(0),
   _segmentAllocator(segmentAllocator),
   _allocatedSegments( SegmentDequeAllocator(rawAllocator) ),
   _providedSegments(std::less< TR::MemorySegment >(), SegmentSetAllocator(rawAllocator)),
   _freeSegments( FreeSegmentDequeAllocator(rawAllocator) ),
   _currentSegment( TR::ref(_segmentAllocator.allocate(segmentAllocateSize) ) )
   {
   TR_ASSERT(defaultSegmentSize <= segmentAllocateSize, "defaultSegmentSize should be smaller than or equal to segmentAllocateSize");

   // We cannot simply assign segmentAllocateSize to _segmentAllocateSize because:
   // We want to make sure that _currentSegment is always a small segment, i.e. its size <= _segmentAllocateSize. When
   // size alignment happens in _segmentAllocator.allocate, this will be violated, make it _currentSegment a large
   // segment capable of allocating large and small segments. A large segment and its containing backing segment
   // is allocated/released differently, we don't want to have a backing segment whose memory is used by a mix of small segments
   // and large segments.
   //
   _segmentAllocateSize = _currentSegment.get().size();

   try
      {
      _allocatedSegments.push_back(TR::ref(_currentSegment));
      }
   catch (...)
      {
      _segmentAllocator.deallocate(_currentSegment);
      throw;
      }
   _bytesAllocated += _segmentAllocateSize;
   }

OMR::SegmentProvider::~SegmentProvider() throw()
   {
   while (!_allocatedSegments.empty())
      {
      BackingSegment &topSegment = _allocatedSegments.back().get();
      _allocatedSegments.pop_back();
      _segmentAllocator.deallocate(topSegment);
      }
   }

TR::MemorySegment &
OMR::SegmentProvider::request(size_t requiredSize)
   {
   size_t const roundedSize = round(requiredSize);

   // first try to find any available free default-sized segments to fulfil the request
   if (
      !_freeSegments.empty()
      && !(defaultSegmentSize() < roundedSize)
      )
      {
      TR::MemorySegment &recycledSegment = _freeSegments.back().get();
      _freeSegments.pop_back();
      recycledSegment.reset();
      return recycledSegment;
      }

   // if not, then try to carve space from current backing segment
   size_t remainingSpace = _segmentAllocator.remaining(_currentSegment);
   if (remainingSpace >= roundedSize)
      {
      // Only provide small segments from _currentSegment
      TR_ASSERT(!isLargeSegment(remainingSpace), "_currentSegment must be a small segment");
      return provideNewSegment(roundedSize, _currentSegment);
      }

   // provider cannot fulfil by itself: need to allocate another backing segment
   size_t sizeToAllocate = std::max(roundedSize, _segmentAllocateSize);
   if (_allocationLimit > 0 && _bytesAllocated + sizeToAllocate > _allocationLimit)
      {
      throw std::bad_alloc();
      }

   BackingSegment &newSegment = _segmentAllocator.allocate(sizeToAllocate);
   TR::reference_wrapper<BackingSegment> newSegmentRef = TR::ref(newSegment);

   try
      {
      _allocatedSegments.push_back(newSegmentRef);
      }
   catch (...)
      {
      _segmentAllocator.deallocate(newSegment);
      throw;
      }
   _bytesAllocated += sizeToAllocate;

   // Rounded size determines when the segment is released, see ::release
   if (!isLargeSegment(roundedSize))
      {
      // _currentSegment may still have some space available in it even though it couldn't fulfil this request
      // carve any such remaining space into small segments and add them to the free segment list so that we can use it later
      //
      while (_segmentAllocator.remaining(_currentSegment) >= defaultSegmentSize())
         {
         _freeSegments.push_back( TR::ref(provideNewSegment(defaultSegmentSize(), _currentSegment) ) );
         }

      _currentSegment = newSegmentRef;
      }
   else
      {
      // _currentSegment should not point to any segment with space larger than _segmentAllocateSize because
      // such segment cannot be reused for other requests and is to be released when the release method is invoked,
      // e.g. when a TR::Region goes out of scope
      //
      }

   return provideNewSegment(roundedSize, newSegmentRef);
   }

void
OMR::SegmentProvider::release(TR::MemorySegment & segment) throw()
   {
   size_t const segmentSize = segment.size();
   if (segmentSize == defaultSegmentSize())
      {
      try
         {
         _freeSegments.push_back(TR::ref(segment));
         }
      catch (...)
         {
         // not much we can do here except leak
         }
      }
   else if (isLargeSegment(segmentSize))
      {
      // Segments larger than _segmentAllocateSize can only provide only one segment,
      // so just release the backing segment
      for (auto i = _allocatedSegments.begin(); i != _allocatedSegments.end(); ++i)
         {
         if (_segmentAllocator.matchesSegment(i->get(), segment)) // s.heapBase == segment.base())
            {
            _bytesProvided -= segmentSize;
            _bytesAllocated -= segmentSize;

            /* Removing segment from _providedSegments */
            auto it = _providedSegments.find(segment);
            _providedSegments.erase(it);

            auto &customSegment = i->get();
            _allocatedSegments.erase(i);
            _segmentAllocator.deallocate(customSegment);
            break;
            }
         }
      }
   else
      {
      size_t const oldSegmentSize = segmentSize;
      void * const oldSegmentArea = segment.base();

      // segment should no longer be maintained in _providedSegments
      auto it = _providedSegments.find(segment);
      _providedSegments.erase(it);

      // split up into default sized chunks and add to free list
      TR_ASSERT(oldSegmentSize % defaultSegmentSize() == 0, "misaligned segment");
      size_t const chunks = oldSegmentSize / defaultSegmentSize();
      for (size_t i = 0; i < chunks; ++i)
         {
         try
            {
            createSegmentFromArea(defaultSegmentSize(), static_cast<uint8_t *>(oldSegmentArea) + (defaultSegmentSize() * i));
            }
         catch (...)
            {
            // not much we can do here except leak
            }
         }
      }
   }

TR::MemorySegment &
OMR::SegmentProvider::provideNewSegment(size_t size, TR::reference_wrapper<BackingSegment> segment)
   {
   TR_ASSERT( (size % defaultSegmentSize()) == 0, "Misaligned segment");
   void *newSegmentArea = operator new(size, segment.get());
   if (!newSegmentArea) throw std::bad_alloc();
   try
      {
      TR::MemorySegment &newSegment = createSegmentFromArea(size, newSegmentArea);
      _bytesProvided += size;
      return newSegment;
      }
   catch (...)
      {
      ::operator delete(newSegmentArea, segment);
      throw;
      }
   }

TR::MemorySegment &
OMR::SegmentProvider::createSegmentFromArea(size_t size, void *newSegmentArea)
   {
   auto result = _providedSegments.insert( TR::MemorySegment(newSegmentArea, size) );
   TR_ASSERT(result.first != _providedSegments.end(), "Bad iterator");
   TR_ASSERT(result.second, "Insertion failed");
   return const_cast<TR::MemorySegment &>(*(result.first));
   }

size_t
OMR::SegmentProvider::round(size_t requestedSize)
   {
   return ( ( ( requestedSize + (defaultSegmentSize() - 1) ) / defaultSegmentSize() ) * defaultSegmentSize() );
   }
