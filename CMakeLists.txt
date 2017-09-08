###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

message(STATUS "Starting with CMake version ${CMAKE_VERSION}")

# Warn user that CMake support is currently experimental
message(WARNING
"CMake support is currently experimental.\n\
Various components may fail to compile with default configuration options, and some are not even attempted.")

include(cmake/versions.cmake) # Required for OMR_VERSION 

project(omr VERSION ${OMR_VERSION} LANGUAGES CXX C)
set(OMR_ROOT ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "Root of OMR")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH})

include(OmrAssert)
include(OmrFindFiles)
include(OmrHookgen)
include(OmrPlatform)
include(OmrTracegen)

###
### Set up the global platform configuration
###

if(NOT OMR_PLATFORM_GLOBALLY_INITIALIZED)
	omr_platform_global_setup()
endif()

###
### Getting the glue target
###
set(OMR_GC_GLUE_TARGET "omr_example_gc_glue" CACHE STRING "The gc glue target, must be interface library")
set(OMR_UTIL_GLUE_TARGET "omr_example_util_glue" CACHE STRING "The util glue target, must be interface library")
set(OMR_RAS_GLUE_TARGET "omr_example_ras_glue" CACHE STRING "The ras glue target, must be interface library")
set(OMR_CORE_GLUE_TARGET "omr_example_core_glue" CACHE STRING "The core glue target, must be and interface library")

# TODO should we always be adding this?  Likely only when compiling OMR proper for testing. Otherwise we would want
# to add the languages glue folder 
add_subdirectory(example/glue)

# TODO: OMR_EXAMPLE flag
# TODO: OMR_RTTI flag

# TODO: Version things

###
### Versions and stuff
###

include(cmake/config.cmake)

enable_testing()

include_directories(
	${PROJECT_BINARY_DIR}
	./include/
	./include_core/
	./third_party/
)

add_definitions(
	-DUT_DIRECT_TRACE_REGISTRATION # TODO:  Deal with that stupid jni issue in tracegen
)

# Check for existing omrcfg in the source tree since this can cause alot of headaches
# Also check if we are building in tree while we are at it
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	if(NOT MSVC_IDE) #MSVC will handle in tree builds ok
		message(WARNING "In tree builds are not recommended")
	endif()
else()
include(cmake/CheckSourceTree.cmake)
add_custom_target(header_check ALL
	${CMAKE_COMMAND} "-Domr_SOURCE_DIR=${omr_SOURCE_DIR}" -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckSourceTree.cmake)
endif()

configure_file(./omrcfg.CMakeTemplate.h omrcfg.h)
configure_file(./omrversionstrings.CMakeTemplate.h omrversionstrings.h)

if(OMR_TOOLS_IMPORTFILE)
	include("${OMR_TOOLS_IMPORTFILE}")
elseif(OMR_TOOLS)
	add_subdirectory(tools)
else()
	message(FATAL_ERROR "OMR: Build tools are required. See OMR_TOOLS and OMR_TOOLS_IMPORTFILE")
endif()

add_subdirectory(thread)
add_subdirectory(port)
add_subdirectory(util)
add_subdirectory(omrtrace)
add_subdirectory(omr)
add_subdirectory(third_party)
add_subdirectory(omrsigcompat)

if(OMR_FVTEST)
	add_subdirectory(fvtest)
endif()

if(OMR_GC)
	add_subdirectory(gc)
endif(OMR_GC)

if(OMR_DDR)
	add_subdirectory(ddr)
endif(OMR_DDR)

if(OMR_JITBUILDER)
	add_subdirectory(jitbuilder)
endif(OMR_JITBUILDER)

