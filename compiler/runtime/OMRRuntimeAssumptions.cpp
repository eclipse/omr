/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "runtime/OMRRuntimeAssumptions.hpp"
#include "env/jittypes.h"

#if (defined(__IBMCPP__) || defined(__open_xl__) && defined(__cplusplus)) && !defined(AIXPPC) && !defined(LINUXPPC)
#define ASM_CALL __cdecl
#else
#define ASM_CALL
#endif

#if defined(TR_HOST_S390) || defined(TR_HOST_X86) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64)
// on these platforms, _patchVirtualGuard is written in C++
extern "C" void _patchVirtualGuard(uint8_t *locationAddr, uint8_t *destinationAddr, int32_t smpFlag);
#else
extern "C" void ASM_CALL _patchVirtualGuard(uint8_t*, uint8_t*, uint32_t);
#endif


void TR::PatchNOPedGuardSite::compensate(bool isSMP, uint8_t *location, uint8_t *destination)
   {
   _patchVirtualGuard(location, destination, isSMP);
   }

TR::PatchSites::PatchSites(TR_PersistentMemory *pm, size_t maxSize) :
    _maxSize(maxSize),
    _size(0),
    _refCount(0),
    _patchPoints(NULL),
    _firstLocation(NULL),
    _lastLocation(NULL)
   {
   size_t bytes = sizeof(uint8_t*) * 2 * maxSize;
   TR_ASSERT(maxSize == bytes/(sizeof(uint8_t*) * 2),  "Requested number of patch sites %d exceeds supported size\n", maxSize);
   _patchPoints = (uint8_t**) pm->jitPersistentAlloc(bytes);
   }

/**
 * Add a patch site to the collection.
 */
void TR::PatchSites::add(uint8_t *location, uint8_t *destination)
   {
   TR_ASSERT_FATAL(_size < _maxSize, "Cannot add more patch sites, max size is %d", _maxSize);
   _patchPoints[_size * 2] = location;
   _patchPoints[_size * 2 + 1] = destination;
   _size++;

   if (_firstLocation == NULL || location < _firstLocation)
      _firstLocation = location;
   if (_lastLocation == NULL || location > _lastLocation)
      _lastLocation = location;
   }

/**
 * Compare this collection with another.
 * Will return true if both will patch the same locations, in the same order.
 * Duplicate locations are also respected.
 */
bool TR::PatchSites::equals(PatchSites *other)
   {
   if (other->getSize() != _size)
      return false;

   if (_firstLocation != other->getFirstLocation() || _lastLocation != other->getLastLocation())
      return false;

   for (size_t i = 0; i < _size; ++i)
      {
      if (getLocation(i) != other->getLocation(i))
         return false;
      }
   return true;
   }

/**
 * Check if a location is contained within this collection.
 */
bool TR::PatchSites::containsLocation(uint8_t *location)
   {
   if (_firstLocation > location || _lastLocation < location)
      return false;
   return internalContainsLocation(location);
   }

bool TR::PatchSites::internalContainsLocation(uint8_t *location)
   {
   for (size_t i = 0; i < _size; ++i) 
      {
      if (getLocation(i) == location)
         return true;
      }
   return false;
   } 

uint8_t *TR::PatchSites::getLocation(size_t index)
   {
   TR_ASSERT(index < _size, "Invalid index for patch site %d but size %d", index, _size);
   return _patchPoints[index * 2];
   }
uint8_t *TR::PatchSites::getDestination(size_t index)
   {
   TR_ASSERT(index < _size, "Invalid index for patch site %d but size %d", index, _size);
   return _patchPoints[index * 2 + 1];
   }

/**
 * Functions to maintain reference counts from runtime assumptions.
 * Each new runtime assumption using the list should call addReferences
 * and should then call reclaim when the runtime assumption is being removed.
 */
void TR::PatchSites::addReference()
   {
   _refCount++;
   TR_ASSERT(_refCount > 0, "Reference count overflow");
   }

void TR::PatchSites::reclaim(PatchSites *sites)
   {
   TR_ASSERT(sites->_refCount > 0, "Attempt to reclaim patch sites without a reference");
   sites->_refCount--;

   if (sites->_refCount == 0)
      {
      TR_PersistentMemory::jitPersistentFree(sites->_patchPoints);
      TR_PersistentMemory::jitPersistentFree(sites);
      }
   }
