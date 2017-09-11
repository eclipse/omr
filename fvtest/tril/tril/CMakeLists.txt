###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

if(NOT TARGET tril)

project(tril LANGUAGES C CXX)

find_package(BISON)
find_package(FLEX)

set(OMR_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../../omr)
set(JITBUILDER_PATH ${OMR_PATH}/jitbuilder)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

BISON_TARGET(tril_parser tril.y ${CMAKE_CURRENT_BINARY_DIR}/tril.parser.c)
FLEX_TARGET(tril_scanner tril.l ${CMAKE_CURRENT_BINARY_DIR}/tril.scanner.c
    COMPILE_FLAGS "--yylineno --header-file=${CMAKE_CURRENT_BINARY_DIR}/tril.scanner.h")
#            DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/tril.scanner.h)
ADD_FLEX_BISON_DEPENDENCY(tril_scanner tril_parser)

add_library(tril STATIC
    ${BISON_tril_parser_OUTPUTS}
    ${FLEX_tril_scanner_OUTPUTS}
    ast.cpp
    ilgen.cpp
    jitbuilder_compiler.cpp
)

target_compile_definitions(tril PUBLIC
    BITVECTOR_BIT_NUMBERING_MSB
    UT_DIRECT_TRACE_REGISTRATION
    JITTEST
    JITBUILDER_SPECIFIC
    PROD_WITH_ASSUMES
    TR_HOST_X86
    TR_HOST_64BIT
    BITVECTOR_64BIT
    LINUX
    TR_TARGET_X86
    TR_TARGET_64BIT
    SUPPORTS_THREAD_LOCAL
    _LONG_LONG
)

target_include_directories(tril PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JITBUILDER_PATH}/release/include
    ${JITBUILDER_PATH}/x
    ${JITBUILDER_PATH}
    ${OMR_PATH}/compiler/x/amd64
    ${OMR_PATH}/compiler/x
    ${OMR_PATH}/compiler
    ${OMR_PATH}
    ${OMR_PATH}/include_core
)

# not very robust but since we can't build JitBuilder by itself using cmake yet,
# this will have to do
if(NOT "${CMAKE_PROJECT_NAME}" STREQUAL "omr")
    set(OMR_JITBUILDER ON CACHE BOOL "")
    set(OMR_WARNINGS_AS_ERRORS OFF CACHE BOOL "")
    add_subdirectory(${OMR_PATH} ${CMAKE_CURRENT_BINARY_DIR}/omr)
endif()
target_link_libraries(tril INTERFACE
    jitbuilder
    dl
)

add_executable(tril_dumper compiler.cpp) 
target_link_libraries(tril_dumper tril)

add_executable(tril_compiler compiler.cpp) 
target_link_libraries(tril_compiler tril)
#export(TARGETS tril jitbuilder FILE tril-config.cmake)
#install(FILES ast.h ilgen.hpp DESTINATION include)
#install(TARGETS tril EXPORT tril-targets ARCHIVE DESTINATION lib)
#install(EXPORT tril-targets FILE tril-config.cmake DESTINATION lib/cmake/tril)
endif()
