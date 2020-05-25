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
#############################################################################

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	find_package(LLVM 8 REQUIRED CONFIG
		PATHS "/usr/local/opt/llvm\@8/bin/:${PATH}"
	)
else()
	find_package(LLVM 8 REQUIRED CONFIG)
endif()

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_library(lljb SHARED
	src/Compiler.cpp
	src/MethodBuilder.cpp
	src/Module.cpp
	src/IRVisitor.cpp
)

target_link_libraries(lljb
	PUBLIC
		jitbuilder
		LLVMIRReader
		LLVMCore
		LLVMSupport
)

target_include_directories(lljb
	PUBLIC
		include/
		${omr_SOURCE_DIR}/compiler
		${omr_SOURCE_DIR}
		${LLVM_INCLUDE_DIRS}
)
