###############################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
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
###############################################################################

# TODO: OMR_RTTI flag
# TODO: Version things

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

message(STATUS "Starting with CMake version ${CMAKE_VERSION}")

include(cmake/versions.cmake) # Required for OMR_VERSION

project(omr VERSION ${OMR_VERSION} LANGUAGES CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH})

set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)

include(OmrAssert)
include(OmrFindFiles)
include(OmrHookgen)
include(OmrOption)
include(OmrPlatform)
include(OmrTracegen)
include(OmrTargetSupport)
include(OmrSanitizerSupport)

###
### Set up the global platform configuration
###

if(NOT OMR_PLATFORM_GLOBALLY_INITIALIZED)
	omr_platform_global_setup()
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

###
### Set up install paths
###
include(GNUInstallDirs)

set(OMR_INSTALL_BIN_DIR ${CMAKE_INSTALL_BINDIR} CACHE PATH "Installation directory for executables")
set(OMR_INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR} CACHE PATH "Installation directory for libraries")
set(OMR_INSTALL_ARCHIVE_DIR ${CMAKE_INSTALL_LIBDIR} CACHE PATH "Installation directory for libraries")
set(OMR_INSTALL_INC_DIR ${CMAKE_INSTALL_INCLUDEDIR} CACHE PATH "Installation directory for headers")
set(OMR_INSTALL_DATA_DIR ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME} CACHE PATH "Installation directory for data files")

###
### Versions and stuff
###

include(cmake/config.cmake)

enable_testing()

###
### omr_base: basic includes and definitions needed everywhere
###

omr_add_library(omr_base INTERFACE)

target_include_directories(omr_base
	INTERFACE
		$<BUILD_INTERFACE:${omr_SOURCE_DIR}/include_core>
		$<BUILD_INTERFACE:${omr_BINARY_DIR}>
		$<INSTALL_INTERFACE:${OMR_INSTALL_INC_DIR}>
)

if(OMR_OS_ZOS)
	if(OMR_USE_NATIVE_ENCODING)
		target_link_libraries(omr_base INTERFACE omr_ebcdic)
		target_compile_definitions(omr_base
			INTERFACE
				-DOMR_EBCDIC
		)
	else()
		target_link_libraries(omr_base INTERFACE omr_ascii)
	endif()
endif()

if(OMR_WINDOWS_NOMINMAX)
	target_compile_definitions(omr_base INTERFACE -DNOMINMAX)
endif()

install(
	TARGETS omr_base
	EXPORT OmrTargets
)

###
### Source tree checks
###

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

configure_file(include_core/omrcfg.cmake.h.in omrcfg.h)
configure_file(include_core/omrversionstrings.cmake.h.in omrversionstrings.h)


###
### Set up DDR configuration
###
include(OmrDDRSupport)
if(OMR_DDR)
	make_ddr_set(omrddr)
	ddr_add_headers(omrddr
		${omr_BINARY_DIR}/omrcfg.h
		include_core/j9nongenerated.h
	)
	set_property(TARGET omrddr PROPERTY INCLUDE_DIRECTORIES
		${CMAKE_CURRENT_SOURCE_DIR}/include_core
		${CMAKE_CURRENT_BINARY_DIR}
	)
endif()

###
### Native Tooling
###

if(OMR_TOOLS_IMPORTFILE)
	include("${OMR_TOOLS_IMPORTFILE}")
elseif(OMR_TOOLS)
	add_subdirectory(tools)
else()
	message(FATAL_ERROR "OMR: Build tools are required. See OMR_TOOLS and OMR_TOOLS_IMPORTFILE")
endif()


###
### Built-in OMR Applications.
###

# Each application provides it's own glue, so must be included before subdirectories.
# The example and om are not yet compatible.
if (OMR_EXAMPLE)
	add_subdirectory(example)
endif (OMR_EXAMPLE)

###
### Core OMR components
###

add_subdirectory(thread)
add_subdirectory(port)
add_subdirectory(util)
add_subdirectory(omr)
add_subdirectory(third_party EXCLUDE_FROM_ALL)
add_subdirectory(omrsigcompat)

if(OMR_RAS_TDF_TRACE)
	add_subdirectory(omrtrace)
endif(OMR_RAS_TDF_TRACE)

if(OMR_GC)
	add_subdirectory(gc)
endif(OMR_GC)

if(OMR_DDR)
	add_subdirectory(ddr)
endif(OMR_DDR)

if(OMR_COMPILER)
	add_subdirectory(compiler)
endif()

if(OMR_JITBUILDER)
	add_subdirectory(jitbuilder)
endif(OMR_JITBUILDER)

# This should come last to ensure dependencies
# are defined
if(OMR_FVTEST)
	add_subdirectory(fvtest)
endif()


# Export CMake Module

# This file must be copied into the build directory so that
# the cmake user package registry functions properly
configure_file(OmrConfig.cmake
  "${CMAKE_CURRENT_BINARY_DIR}/OmrConfig.cmake"
  COPYONLY
)

export(EXPORT OmrTargets FILE OmrTargets.cmake)

install(EXPORT OmrTargets
  FILE OmrTargets.cmake
  DESTINATION ${OMR_INSTALL_LIB_DIR}/cmake/Omr
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/OmrConfig.cmake"
	DESTINATION ${OMR_INSTALL_LIB_DIR}/cmake/Omr
)

export(PACKAGE Omr)
