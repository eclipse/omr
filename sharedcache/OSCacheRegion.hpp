/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#if !defined(OSCACHE_REGION_HPP_INCLUDED)
#define OSCACHE_REGION_HPP_INCLUDED

#include "omr.h"

#include "OSCacheMemoryProtector.hpp"
#include "OSCacheRegionInitializer.hpp"
#include "OSCacheRegionSerializer.hpp"

/* C macros are the fuckin' worst, but here's a few from J9 anyway. */
#define ROUND_UP_TO(granularity, number) ( (((number) % (granularity)) ? ((number) + (granularity) - ((number) % (granularity))) : (number)))
#define ROUND_DOWN_TO(granularity, number) ( (((number) % (granularity)) ? ((number) - ((number) % (granularity))) : (number)))

class OSCacheImpl;
class OSCacheLayout;
class OSCacheRegionMemoryProtector;

class OSCacheRegion {
public:
  OSCacheRegion(OSCacheLayout* layout, int regionID)
    : _layout(layout)
    , _regionID(regionID)
  {}

  // the start address of the region.
  virtual void* regionStartAddress() const = 0;

  // does the described address of data fall within the region's memory?
  virtual bool isAddressInRegion(void* itemAddress) = 0;

  // does the described block of data fall within the region's memory?
  virtual bool isBlockInRegion(void* itemAddress, UDATA itemSize) = 0;

  // render the Region's memory protection settings as flags that can
  // be used by the underlying components of the OMRPortLibrary, as
  // anticipated by the OSCacheRegion subclass.
  virtual UDATA renderToMemoryProtectionFlags() = 0;

  // this is not a predicate, but it may modify state. If the
  // subclass isn't concerned with aligning to page boundaries, it can
  // just as well be a no-op. returns true if re-alignment was
  // attempted, and succeeded.
  virtual bool alignToPageBoundary(UDATA osPageSize) = 0;

  // check the cache region for corruption using whatever means are
  // available.  I'm not sure this method should belong to the
  // OSCacheRegion class.  OSCacheConfig may be a more appropriate
  // choice.
  virtual IDATA checkValidity() = 0;

  virtual void serialize(OSCacheRegionSerializer* serializer) = 0;

  virtual void initialize(OSCacheRegionInitializer* initializer) = 0;

  virtual bool adjustRegionStartAndSize(void* blockAddress, uintptr_t size) = 0;

  // calculates the size of the region.
  virtual UDATA regionSize() const = 0;

  virtual int regionID() {
    return _regionID;
  }

  // add memory protections to the region.
  virtual IDATA setPermissions(OSCacheMemoryProtector* protector) = 0;

  virtual U_32 computeCRC(U_32 seed, U_32 stepSize) = 0;

  virtual void setCacheLayout(OSCacheLayout* layout) {
    _layout = layout;
  }

protected:
  OSCacheLayout* _layout;
  int _regionID;
};

#endif
