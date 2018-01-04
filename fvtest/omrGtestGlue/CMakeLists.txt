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

add_library(omrGtest STATIC
	omrGtest.cpp)
target_include_directories(omrGtest
	PRIVATE
		${PROJECT_SOURCE_DIR}/third_party/gtest-1.8.0/
		${PROJECT_SOURCE_DIR}/third_party/gtest-1.8.0/src
	PUBLIC
		${PROJECT_SOURCE_DIR}/third_party/gtest-1.8.0/include
)
add_library(omrGtestGlue INTERFACE)

target_include_directories(omrGtestGlue
	INTERFACE
		.
)

target_sources(omrGtestGlue INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/argmain.cpp
)

target_link_libraries(omrGtestGlue INTERFACE omrGtest)

#TODO  system thread library should be linked in a more generic way.
if(NOT OMR_HOST_OS STREQUAL "win")
	if(NOT OMR_HOST_OS STREQUAL "zos")
		target_link_libraries(omrGtest pthread)
	endif()
endif()
#target_link_libraries(omrGtest INTERFACE omrGtestGlue)

set_property(TARGET omrGtest PROPERTY FOLDER fvtest)
