################################################################################
# Copyright (c) 2020, 2021 IBM Corp. and others
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
include(OmrCompilerSupport)

omr_add_tracegen(include/omrshr.tdf)

add_library(sharedcache STATIC
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

target_include_directories(sharedcache
	PRIVATE
		${PROJECT_SOURCE_DIR}/nls
		${PROJECT_SOURCE_DIR}/include_core
		${PROJECT_SOURCE_DIR}/port/unix
		${PROJECT_SOURCE_DIR}/sharedcache
	PUBLIC
		${PROJECT_SOURCE_DIR}/compiler/${TR_TARGET_ARCH}/${TR_TARGET_SUBARCH}
		${PROJECT_SOURCE_DIR}/compiler/${TR_TARGET_ARCH}
		${PROJECT_SOURCE_DIR}/compiler
		${PROJECT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR}
		include
		.
)

target_link_libraries(sharedcache
	PRIVATE
		j9pool
		j9hashtable
)

target_compile_options(sharedcache PUBLIC -fexceptions)
