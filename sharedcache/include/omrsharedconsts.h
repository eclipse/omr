/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
#ifndef OMRSHAREDCONSTS_H_
#define OMRSHAREDCONSTS_H_

#define OMR_PATHSEP DIR_SEPARATOR_STR

#define OMR_CACHE_ROOT_PREFIX "sharedcache"

/* Maximum length of the cache name (including NULL char) specified by user in the command line */
#define OMR_USER_SPECIFIED_CACHE_NAME_MAXLEN 65

/*
 * For Windows,  cacheNameWithVersionString
 * = version string + prefix separator char + cacheName + prefix separator char + generation string
 *
 * For Unix,  cacheNameWithVersionString
 * = version string + identifier string + cacheName + prefix separator char + generation string
 *
 * Pick the unix version since it has the maximum possible length for cacheNameWithVersionString
 * compared to windows version for the same cacheName.
 *
 * length of version string =>  11 chars
 * identifier string is "_memory_" length => 8 chars
 * maximum length of user specified or default cacheName  => 64 chars
 * prefix separator char is _     => 1 char
 * generation string is of form Gnn where nn stands for 2 digit number => 3 chars
 *
 * Total chars needed  = 11 + 8 + 64 + 1 + 3 = 87 chars.
 *
 * Add 1 to above length to accomodate NULL char.
 * Reference:  getCacheVersionAndGen function
 */
#define OMR_CACHE_ROOT_MAXLEN 88
 
#define OMR_OSCACHE_DATA_WRITE_LOCK 1
#define OMR_OSCACHE_DATA_READ_LOCK 2

#define OMR_OSCACHE_OPEN_MODE_DO_READONLY 0x1
#define OMR_OSCACHE_OPEN_MODE_TRY_READONLY_ON_FAIL 0x2
#define OMR_OSCACHE_OPEN_MODE_GROUPACCESS 0x8
#define OMR_OSCACHE_OPEN_MODE_CHECKBUILDID 0x10
#define OMR_OSCACHE_OPEN_MODE_CHECK_NETWORK_CACHE 0x20

#define OMR_CACHE_FILE_MODE_USERDIR_WITH_GROUPACCESS 0664
#define OMR_CACHE_FILE_MODE_USERDIR_WITHOUT_GROUPACCESS 0644
#define OMR_CACHE_FILE_MODE_DEFAULTDIR_WITH_GROUPACCESS 0660
#define OMR_CACHE_FILE_MODE_DEFAULTDIR_WITHOUT_GROUPACCESS 0600

/* Codes to indicate cause of corruption */
#define OMR_NO_CORRUPTION 0
#define OMR_CACHE_CRC_INVALID -1
#define OMR_CACHE_HEADER_BAD_EYECATCHER -8
#define OMR_ACQUIRE_HEADER_WRITE_LOCK_FAILED -10
#define OMR_CACHE_SEMAPHORE_MISMATCH -19

/**
 * This enum contains constants that to indicate if the cache is accessible by current user or not.
 * It is used when printing cache stats.
 */
typedef enum OMR_CacheAccess {
    OMR_CACHE_ACCESS_ALLOWED = 0,
    OMR_CACHE_ACCESS_ALLOWED_WITH_GROUPACCESS,
    OMR_CACHE_ACCESS_ALLOWED_WITH_GROUPACCESS_READONLY,
    OMR_CACHE_ACCESS_NOT_ALLOWED
} OMR_CacheAccess;

#endif /*OMRSHAREDCONSTS_H_*/
