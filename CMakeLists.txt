###############################################################################
#
# (c) Copyright IBM Corp. 2017
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
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

###
### Getting the glue target
###
if (NOT DEFINED OMR_GLUE_TARGET)
	set(OMR_GLUE_TARGET "omr_example_glue" CACHE STRING "The glue target, must be an interface library")
    message(STATUS "Glue not set, defaulting to example glue")
endif()
add_subdirectory(example/glue)

# TODO: OMR_EXAMPLE flag
# TODO: OMR_RTTI flag

# TODO: Version things

###
### Versions and stuff
###

include(cmake/platform.cmake)
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

add_library(omrglue STATIC)
# Link as private since we dont want the interface sources to propagate
target_link_libraries(omrglue
	PUBLIC
	omrgc
	PRIVATE
	${OMR_GLUE_TARGET}
)
#but we need to propagate the include paths
target_include_directories(omrglue INTERFACE $<TARGET_PROPERTY:${OMR_GLUE_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)

add_subdirectory(tools)
# Yeah, its dumb doing this here. Read note in tools/CMakeLists.txt
if(CMAKE_CROSSCOMPILING)
	include(${OMR_TOOLS_IMPORTFILE})
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
  # if we are building the gc, add gc headers to omrglue
  target_include_directories(omrglue PRIVATE $<TARGET_PROPERTY:omrgc,INCLUDE_DIRECTORIES>)
endif(OMR_GC)

if(OMR_DDR)
  add_subdirectory(ddr)
endif(OMR_DDR)

if(OMR_JITBUILDER)
	add_subdirectory(jitbuilder)
endif(OMR_JITBUILDER)

