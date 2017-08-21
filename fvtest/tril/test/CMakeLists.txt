###############################################################################
#
# (c) Copyright IBM Corp. 2017, 2017
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
   SymRefTabTest.cpp
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

