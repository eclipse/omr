################################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
################################################################################

omr_add_tracegen(omrshr.tdf)

add_library(shared_cache STATIC
  OSCacheConfigOptions.cpp
  OSCacheImpl.cpp
  OSCacheUtils.cpp
  OSMemoryMappedCacheIterator.cpp
  OSMemoryMappedCache.cpp
  OSMemoryMappedCacheAttachingContext.cpp
  OSMemoryMappedCacheCreatingContext.cpp
  OSMemoryMappedCacheConfig.cpp
  OSMemoryMappedCacheHeader.cpp
  OSMemoryMappedCacheStats.cpp
  OSMemoryMappedCacheUtils.cpp
  OSSharedMemoryCacheIterator.cpp
  OSSharedMemoryCachePolicies.cpp
  OSSharedMemoryCache.cpp  
  OSSharedMemoryCacheConfig.cpp
  OSSharedMemoryCacheHeader.cpp
  OSSharedMemoryCacheSnapshot.cpp
  OSSharedMemoryCacheStats.cpp
  OSSharedMemoryCacheUtils.cpp
  CacheCRCChecker.cpp
  SynchronizedCacheCounter.cpp
  OSCacheLayout.cpp
  OSCacheContiguousRegion.cpp
  OSMemoryMappedCacheMemoryProtector.cpp
  OSSharedMemoryCacheMemoryProtector.cpp  
  ${CMAKE_CURRENT_BINARY_DIR}/ut_omrshr.c
)

target_include_directories(shared_cache
	PRIVATE
		${j9vm_SOURCE_DIR}/shared
		${j9vm_SOURCE_DIR}/shared_util
		${j9vm_SOURCE_DIR}/verbose
		${PROJECT_SOURCE_DIR}/nls
		${PROJECT_SOURCE_DIR}/include_core
		${PROJECT_SOURCE_DIR}/runtime/port/sysvipc
		${PROJECT_SOURCE_DIR}/port/sysvipc
		${PROJECT_SOURCE_DIR}/runtime/shared_common/modifications
	PUBLIC
		${PROJECT_SOURCE_DIR}/compiler
		${CMAKE_CURRENT_BINARY_DIR}
		include		.
)

 target_link_libraries(shared_cache
 	PRIVATE
# 		j9vm_interface
 		j9pool
 		j9hashtable
# 		j9utilcore
# 		j9util
)
