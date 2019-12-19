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

#if !defined(OS_SHARED_MEMORY_CACHE_SNAPSHOT_HPP_INCLUDED)
#define OS_SHARED_MEMORY_CACHE_SNAPSHOT_HPP_INCLUDED

class OSSharedMemoryCache;

class OSSharedMemoryCacheSnapshot
{
public:
  OSSharedMemoryCacheSnapshot(OSSharedMemoryCache* cache)
    : _cache(cache)
    , _cacheExists(false)
    , _fd(0)
  {}  

  // the name of the cache snapshot in the filesystem.
  virtual char* snapshotName() = 0; // replaces cacheNameWithVGen in restoreFromSnapshot.
  // is the file size of the attached file descriptor within the expected bounds?
  virtual bool fileSizeWithinBounds() = 0; // replaces ((fileSize < MIN_CC_SIZE) || (fileSize > MAX_CC_SIZE))
  // once the snapshot file is found to exist, how do we restore it to the local cache object?
  virtual IDATA restoreFromExistingSnapshot() = 0;

  virtual IDATA restoreFromSnapshot(IDATA numLocks);
protected:
  OSSharedMemoryCache* _cache;

  bool _cacheExists;
  IDATA _fd;
};

#endif
