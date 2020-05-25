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

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	find_package(LLVM 8 REQUIRED CONFIG
		PATHS "/usr/local/opt/llvm\@8/bin/:${PATH}"
	)
else()
	find_package(LLVM 8 REQUIRED CONFIG)
endif()

# we need clang to generate IR from C++
find_program(CLANG_EXECUTABLE  NAMES clang++)

if (NOT CLANG_EXECUTABLE)
	message(FATAL_ERROR "Could not find clang++")
endif()

# Generate llvm bitcode files from C++ using clang++
function(generate_module_from_cxx src)
	add_custom_command(
		OUTPUT
			${CMAKE_CURRENT_BINARY_DIR}/${src}.ll
		COMMAND
			${CLANG_EXECUTABLE} -S -emit-llvm -std=c++0x -O0 -o ${CMAKE_CURRENT_BINARY_DIR}/${src}.ll ${CMAKE_CURRENT_SOURCE_DIR}/${src}.cpp
		MAIN_DEPENDENCY
			${CMAKE_CURRENT_SOURCE_DIR}/${src}.cpp
	)
	add_custom_target(generate_module_from_cxx_${src} ALL
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${src}.ll
	)
endfunction(generate_module_from_cxx)

add_executable(lljb_run
	lljb_run.cpp
)

target_link_libraries(lljb_run
	lljb
)

function(add_lljb_test test)
	generate_module_from_cxx(${test})
	add_test(
		NAME lljb_${test}_test
		COMMAND lljb_run ${test}.ll
	)
endfunction(add_lljb_test)

add_lljb_test(add)
add_lljb_test(arrays)
add_lljb_test(conditional)
add_lljb_test(div)
add_lljb_test(doubles)
add_lljb_test(floats)
add_lljb_test(function_call)
add_lljb_test(loops)
add_lljb_test(md5)
add_lljb_test(mul)
add_lljb_test(print_string)
add_lljb_test(structs)
add_lljb_test(sub)
add_lljb_test(ternary)
add_lljb_test(unions)

# the following tests work only on Linux and macOS
if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	add_lljb_test(mandelbrot)
	add_lljb_test(time)
endif()
