###############################################################################
# Copyright IBM Corp. and others 2017
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
###############################################################################

include(OmrFindFiles)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_tracegen(j9thr.tdf)

set(OBJECTS "")
set(VPATHS "")
list(APPEND OBJECTS
	j9sem.c
	omrthread.c
	omrthreadattr.c
	omrthreaddebug.c
	omrthreaderror.c
	omrthreadinspect.c
	omrthreadmem.cpp
	omrthreadnuma.c
	omrthreadpriority.c
	omrthreadtls.c
	priority.c
	thrcreate.c
	threadhelpers.cpp
	thrprof.c
	thrdsup.c
	rasthrsup.c
	rwmutex.c
)

if(OMR_THR_JLM)
	list(APPEND OBJECTS omrthreadjlm.c)
endif(OMR_THR_JLM)

if(NOT OMR_OS_WINDOWS)
	list(APPEND OBJECTS unixpriority.c)
else()
	list(APPEND OBJECTS dllmain.c)
	list(APPEND VPATHS win32)
endif()

include_directories(.)

if(OMR_OS_ZOS)
	list(APPEND VPATHS
		zos390
		unix
	)
	include_directories(zos390 unix)
	list(APPEND OBJECTS thrcputime.s)
endif()

if(OMR_OS_LINUX)
	list(APPEND VPATHS
		linux
		unix
	)
	include_directories(linux unix)
endif()

if(OMR_OS_OSX)
	list(APPEND VPATHS osx unix)
	include_directories(osx unix)
endif()

if(OMR_OS_AIX)
	list(APPEND VPATHS aix unix)
	include_directories(aix unix)
endif()

list(APPEND VPATHS common)

#TODO need to add warning flags
#relevant Makefile section:
# Disable some warnings
#ifeq ($(OMR_TOOLCHAIN),gcc)
#  MODULE_CFLAGS += -Wno-unused
#endif

omr_find_files(resolvedPaths
	PATHS ${VPATHS}
	FILES ${OBJECTS}
)

include_directories(common)

#TODO also should be able to build dynamic lib

omr_add_library(j9thr_obj OBJECT
	${resolvedPaths}
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9thr.c
)

target_include_directories(j9thr_obj
	PUBLIC
		$<TARGET_PROPERTY:omr_base,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(j9thr_obj
	PUBLIC
		$<TARGET_PROPERTY:omr_base,INTERFACE_COMPILE_DEFINITIONS>
)

target_compile_options(j9thr_obj
	PUBLIC
		$<TARGET_PROPERTY:omr_base,INTERFACE_COMPILE_OPTIONS>
)

target_enable_ddr(j9thr_obj)
ddr_add_headers(j9thr_obj
	${omr_SOURCE_DIR}/include_core/omrthread.h
	${omr_SOURCE_DIR}/include_core/omrthread_generated.h
	${omr_SOURCE_DIR}/include_core/thread_api.h
	${omr_SOURCE_DIR}/include_core/thrtypes.h
)
ddr_set_add_targets(omrddr j9thr_obj)

omr_add_library(j9thrstatic STATIC
	$<TARGET_OBJECTS:j9thr_obj>
)

include(exports.cmake)

target_include_directories(j9thrstatic
	INTERFACE
	.
)

target_link_libraries(j9thrstatic
	PUBLIC
		omr_base
		j9pool
		omrutil
		${OMR_PLATFORM_THREAD_LIBRARY}
)

if(OMR_OS_WINDOWS)
	target_link_libraries(j9thrstatic
		PRIVATE
			winmm.lib)
endif()
