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

if(NOT TARGET triltest)

project(triltest LANGUAGES CXX)

# not very robust but since we can't build JitBuilder by itself using cmake yet,
# this will have to do
if(NOT "${CMAKE_PROJECT_NAME}" STREQUAL "omr")
   find_package(GTest REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

add_executable(triltest
   main.cpp
   ASTTest.cpp
   ParserTest.cpp
   MethodInfoTest.cpp
   IlGenTest.cpp
   CompileTest.cpp
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../tril/ ${CMAKE_CURRENT_BINARY_DIR}/tril)
target_link_libraries(triltest
   tril
)

# not very robust but since we can't build JitBuilder by itself using cmake yet,
# this will have to do
if(NOT "${CMAKE_PROJECT_NAME}" STREQUAL "omr")
    target_link_libraries(triltest ${GTEST_BOTH_LIBRARIES})
else()
    target_link_libraries(triltest omrGtest)
endif()

add_test(
    NAME triltest
    COMMAND triltest --gtest_color=yes
)

endif()
