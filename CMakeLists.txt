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

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH})


###
### Getting the glue directory
###

if (NOT DEFINED OMR_GLUE)
    set(OMR_GLUE "./example/glue" CACHE PATH "The glue directory")
    message(STATUS "Glue not set, defaulting to example glue")
endif()

# TODO: OMR_EXAMPLE flag
# TODO: OMR_RTTI flag

# TODO: Version things

###
### Versions and stuff
###

include(cmake/platform.cmake)
include(cmake/config.cmake)
include(cmake/vpath.cmake)

enable_testing()

#include current source dir and current bin dir automatically
set(CMAKE_INCLUDE_CURRENT_DIR ON)

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

#TODO should be wrapped in if()
add_subdirectory(fvtest)

if(OMR_GC)
  add_subdirectory(gc)
  add_subdirectory("${OMR_GLUE}")
endif(OMR_GC)



if(NOT OMR_HOST_OS STREQUAL "win")
    # TODO: JitBuilder built through CMake isn't quite 
    #       ready for windows yet 
    add_subdirectory(jitbuilder)
endif()
