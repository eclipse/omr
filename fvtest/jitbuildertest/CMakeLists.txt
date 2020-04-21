###############################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
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

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(jitbuildertest
	main.cpp
	selftest.cpp
	UnionTest.cpp
	FieldAddressTest.cpp
	AnonymousTest.cpp
	ControlFlowTest.cpp
	NegateTest.cpp
	SystemLinkageTest.cpp
	WorklistTest.cpp
	FieldNameTest.cpp
	ConvertBitsTest.cpp
	SelectTest.cpp
	GlobalTest.cpp
)

if(OMR_HOST_ARCH STREQUAL "x86")
	if(OMR_HOST_OS STREQUAL "linux" OR OMR_HOST_OS STREQUAL "osx")
		target_sources(jitbuildertest PRIVATE CallReturnTest.cpp)
	endif()
endif()

if(NOT OMR_HOST_ARCH STREQUAL "ppc")
	target_sources(jitbuildertest PRIVATE UnsignedDivRemTest.cpp)
endif()

target_link_libraries(jitbuildertest
	jitbuilder
	omrGtest
	${CMAKE_DL_LIBS}
)

set_property(TARGET jitbuildertest PROPERTY FOLDER fvtest)

# Disable JitBuilder tests on RISC-V. This is essentially a workaround
# until #4764 is addressed. Once done, tests should be properly skipped.
if(NOT OMR_HOST_ARCH STREQUAL "riscv")
	add_test(NAME JitBuilderTest COMMAND jitbuildertest --gtest_output=xml:${CMAKE_CURRENT_BINARY_DIR}/jitbuildertest-results.xml)
endif()
