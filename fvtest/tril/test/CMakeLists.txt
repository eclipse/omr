###############################################################################
# Copyright IBM Corp. and others 2017
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
###############################################################################

# cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
#
# project(triltest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

omr_add_executable(triltest NOWARNINGS
	main.cpp
	ASTTest.cpp
	ParserTest.cpp
	MethodInfoTest.cpp
	IlGenTest.cpp
	CompileTest.cpp
	FormatTest.cpp
)

target_link_libraries(triltest
	omrGtestGlue
	tril
	omrGtest
)

# The platform specific ${TR_CXX_COMPILE_OPTIONS} and ${TR_C_COMPILE_OPTIONS} compile options
# should been added, on Windows, for example, these options enable exception handling
# what is required for Clang.
target_compile_options(triltest
	PUBLIC
		${TR_COMPILE_OPTIONS}
		$<$<COMPILE_LANGUAGE:CXX>:${TR_CXX_COMPILE_OPTIONS}>
		$<$<COMPILE_LANGUAGE:C>:${TR_C_COMPILE_OPTIONS}>
)

set_property(TARGET triltest PROPERTY FOLDER fvtest/tril)

omr_add_test(
	NAME triltest
	COMMAND $<TARGET_FILE:triltest> --gtest_output=xml:${CMAKE_CURRENT_BINARY_DIR}/triltest-results.xml
)
