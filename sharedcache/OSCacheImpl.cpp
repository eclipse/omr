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

#include "omrport.h"

#include "OSCacheUtils.hpp"
#include "OSCacheImpl.hpp"

OSCacheImpl::OSCacheImpl(OMRPortLibrary* library,
			 OSCacheConfigOptions* configOptions,
			 IDATA numLocks,
			 char* cacheName,
			 char* cacheLocation)
    : OSCache(configOptions)
    , _portLibrary(library)
    , _numLocks(numLocks)
    , _cacheName(cacheName)
    , _cacheLocation(cacheLocation)
{}

IDATA
OSCacheImpl::initCacheDirName(const char* ctrlDirName)
{
    OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

    char fullPathName[OMRSH_MAXPATH];

    IDATA openMode = _configOptions->openMode();
    IDATA cacheDirPermissions = _configOptions->cacheDirPermissions();

    if (!(_cacheLocation = (char*)omrmem_allocate_memory(OMRSH_MAXPATH, OMRMEM_CATEGORY_CLASSES))) {
        return -1;
    }

    IDATA rc = OSCacheUtils::getCacheDirName(OMRPORTLIB,
                                             ctrlDirName,
                                             _cacheLocation,
                                             OMRSH_MAXPATH,
                                             _configOptions);

    if (rc == -1) {
        return -1;
    }

    rc = OSCacheUtils::createCacheDir(OMRPORTLIB,
                                      _cacheLocation,
                                      cacheDirPermissions,
                                      ctrlDirName == NULL);
    if (rc == -1) {
        /* remove trailing '/' */
        _cacheLocation[strlen(_cacheLocation)-1] = '\0';
        return -1;
    }

    /* In the original commonStartup, there are here segments of code
       that should be provided by overloading subclasses, or handled by
       checks to the configuration object. Also, stuff with generation
       and version labels, and buildIDs. How the cachePathName is
       populated using that information. */
    return 0;
}

/* This is an unhelpful name that describes an obscure feature: the
   ability to disclaim regions of memory from page
   protection. Specifically, if an area of memory is no longer needed
   by a program, the program can use the disclaim64 routine to declare
   it as such to AIX OS. Hence the flag. The omrmmap_dont_need
   routine wraps it inside port/unix/omrmmap.c. If OMR is running on
   Linux or OS X (which is vastly more likely), the call
   madvise((void*)roundedStart, roundedLength, MADV_DONTNEED) is used
   instead (it does the same thing, basically, but because it's an
   "advisement", it shrinks from making hard guarantees). madvise is
   documented here:

   http://man7.org/linux/man-pages/man2/madvise.2.html
*/

void
OSCacheImpl::dontNeedMetadata(const void* startAddress, size_t length)
{
    /* AIX does not allow memory to be disclaimed for memory mapped files */
#if !defined(AIXPPC)
    /* why does it need the specific VM?!? It only appears to use it to get at the
       portLibrary. But the _portLibrary just points to a table of C functions. I can't
       imagine how those addresses might be dependent on the thread or VM objects.
       This seems totally unnecessary to me, since we have _portLibrary at the ready
       in this class. So, I've removed the VM parameter. */
    //OMRPORT_ACCESS_FROM_VMC(currentThread->_vm);
    OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
    omrmmap_dont_need(startAddress, length);
#endif
}

void
OSCacheImpl::commonInit()
{
    _errorCode = 0;
    _runningReadOnly = false;
}

void
OSCacheImpl::commonCleanup()
{
    OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

    Trc_SHR_OSC_commonCleanup_Entry();

    if (_cacheName) {
        omrmem_free_memory(_cacheName);
    }

    if (_cachePathName) {
        omrmem_free_memory(_cachePathName);
    }

    if (_cacheLocation) {
        omrmem_free_memory(_cacheLocation);
    }

    /* If the cache is destroyed and then restarted, we still need portLibrary, versionData and generation */
    commonInit();

    Trc_SHR_OSC_commonCleanup_Exit();
}

/**
 * Sets the protection as specified by flags for the memory pages
 * containing all or part of the interval address->(address+len)
 *
 * @param[in] portLibrary An instance of portLibrary
 * @param[in] address 	Pointer to the shared memory region.
 * @param[in] length	The size of memory in bytes spanning the region in which we want to set protection
 * @param[in] flags 	The specified protection to apply to the pages in the specified interval
 *
 * @return 0 if the operations has been successful, -1 if an error has occured
 */
IDATA
OSCacheImpl::setRegionPermissions(OSCacheRegion* region)
{
    OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

    OSCacheMemoryProtector* protector = constructMemoryProtector();
    return region->setPermissions(protector);
}
