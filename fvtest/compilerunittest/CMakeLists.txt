###############################################################################
# Copyright (c) 2020, 2020 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(compunittest LANGUAGES C CXX)

set(OMR_WARNINGS_AS_ERRORS OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COMPCGTEST_FILES
	main.cpp
)

if(OMR_ARCH_POWER)
	list(APPEND COMPCGTEST_FILES
		p/BinaryEncoder.cpp
		p/MemoryReferenceExpansion.cpp
	)
endif()

omr_add_executable(compunittest ${COMPCGTEST_FILES})

# For the time being, link against Tril even though we don't really need Tril itself. This is done
# to avoid needing to duplicate the boilerplate in Tril's CMakeLists.txt.
target_link_libraries(compunittest
	omrGtestGlue
	tril
)

set_property(TARGET compunittest PROPERTY FOLDER fvtest)

add_test(
	NAME compunittest
	COMMAND compunittest --gtest_color=yes --gtest_output=xml:${CMAKE_CURRENT_BINARY_DIR}/compunittest-results.xml
)
