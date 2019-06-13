###############################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
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

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/modules"
	${CMAKE_MODULE_PATH})

include(OmrUtility)
include(FindYACC)
include(FindLEX)

find_package(YACC REQUIRED)
find_package(LEX REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND TRIL_FLEX_FLAGS)

# There is no unistd.h on Windows, so if win_flex.exe is in use,
# the --wincompat option must be added.
if(OMR_HOST_OS STREQUAL "win")
	list(APPEND TRIL_FLEX_FLAGS
		"--wincompat"
	)
endif()

omr_stringify(TRIL_FLEX_OPTIONS ${TRIL_FLEX_FLAGS})
YACC_TARGET(tril_parser tril.y ${CMAKE_CURRENT_BINARY_DIR}/tril.parser.c
	DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/tril.parser.h)
LEX_TARGET(tril_scanner tril.l ${CMAKE_CURRENT_BINARY_DIR}/tril.scanner.c
	COMPILE_FLAGS ${TRIL_FLEX_OPTIONS}
)
ADD_LEX_YACC_DEPENDENCY(tril_scanner tril_parser)

set(TRIL_BACKEND_LIB      testcompiler)

add_library(tril STATIC
	${YACC_tril_parser_OUTPUTS}
	${LEX_tril_scanner_OUTPUTS}
	ast.cpp
	ilgen.cpp
	simple_compiler.cpp
)

# This is mildly peculiar. Because tril relies
# on jitbuilder as a 'backend' of sorts, we use
# its includes. Tril exports this as interface
make_compiler_target(tril PUBLIC COMPILER ${TRIL_BACKEND_LIB})

target_include_directories(tril PUBLIC
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tril PUBLIC
	${TRIL_BACKEND_LIB}
	${CMAKE_DL_LIBS}
)

add_executable(tril_dumper compiler.cpp)
target_link_libraries(tril_dumper tril)

add_executable(tril_compiler compiler.cpp)
target_link_libraries(tril_compiler tril)

# The platform specific ${TR_CXX_COMPILE_OPTIONS} and ${TR_C_COMPILE_OPTIONS} compile options
# should been added, on Windows, for example, these options enable exception handling
# what is required for Clang.
target_compile_options(tril
	PUBLIC
		${TR_COMPILE_OPTIONS}
		$<$<COMPILE_LANGUAGE:CXX>:${TR_CXX_COMPILE_OPTIONS}>
		$<$<COMPILE_LANGUAGE:C>:${TR_C_COMPILE_OPTIONS}>
)

#export(TARGETS tril jitbuilder FILE tril-config.cmake)
#install(FILES ast.h ilgen.hpp DESTINATION include)
#install(TARGETS tril EXPORT tril-targets ARCHIVE DESTINATION lib)
#install(EXPORT tril-targets FILE tril-config.cmake DESTINATION lib/cmake/tril)

set_target_properties(tril tril_dumper tril_compiler PROPERTIES FOLDER fvtest/tril)
