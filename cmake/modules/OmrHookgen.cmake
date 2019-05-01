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

if(OMR_HOOKGEN_)
	return()
endif()
set(OMR_HOOKGEN_ 1)
include(CMakeParseArguments)

# Process an input hookgen file to generate output source files.
# Usage: (INPUT <input> [PRIVATE_DIR <private_header_dir>] [PUBLIC_DIR <public_header_dir>]
# public headers are written to public_header_dir (defaults to current binary dir)
# private headers are written to private_header_dir (defaults to current binary dir)
function(omr_add_hookgen)
	cmake_parse_arguments(OPT "" "INPUT;PRIVATE_DIR;PUBLIC_DIR" "" ${ARGN})

	if(NOT OPT_INPUT)
		message(FATAL_ERROR "value for INPUT must be specified")
	endif()

	#if private_dir or public_dir not specified, default to CMAKE_CURRENT_BINARY_DIR
	if(NOT OPT_PRIVATE_DIR)
		set(OPT_PRIVATE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
	endif()
	if(NOT OPT_PUBLIC_DIR)
		set(OPT_PUBLIC_DIR "${CMAKE_CURRENT_BINARY_DIR}")
	endif()

	get_filename_component(input_filename "${OPT_INPUT}" NAME)

	file(READ "${OPT_INPUT}" hook_def)
	string(REGEX MATCH "<privateHeader>[^<]*" private_header "${hook_def}")
	string(REGEX MATCH "<publicHeader>[^<]*" public_header "${hook_def}")
	if((NOT public_header) OR (NOT private_header))
		message(FATAL_ERROR "Header extraction failed for ${input}")
	endif()

	string(REPLACE "<privateHeader>" "" private_header "${private_header}")
	string(STRIP "${private_header}" private_header)
	get_filename_component(private_header "${private_header}" NAME)

	string(REPLACE "<publicHeader>" "" public_header "${public_header}")
	string(STRIP "${public_header}" public_header)
	get_filename_component(public_header "${public_header}" NAME)

	# Hookgen does not accept absolute paths, they must be relative to the input file
	# So we need to figure out what that would be
	file(RELATIVE_PATH private_path "${CMAKE_CURRENT_BINARY_DIR}" "${OPT_PRIVATE_DIR}/${private_header}")
	file(RELATIVE_PATH public_path "${CMAKE_CURRENT_BINARY_DIR}" "${OPT_PUBLIC_DIR}/${public_header}")

	string(REGEX REPLACE "<privateHeader>[^<]*" "<privateHeader>${private_path}" hook_def "${hook_def}")
	string(REGEX REPLACE "<publicHeader>[^<]*" "<publicHeader>${public_path}" hook_def "${hook_def}")

	file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${input_filename}" "${hook_def}")

	# Make cmake step depend on .hdf file, since we need to regen the modified .hdf
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OPT_INPUT}")

	add_custom_command(
		OUTPUT "${OPT_PRIVATE_DIR}/${private_header}" "${OPT_PUBLIC_DIR}/${public_header}"
		COMMAND ${OMR_EXE_LAUNCHER} $<TARGET_FILE:hookgen> "${CMAKE_CURRENT_BINARY_DIR}/${input_filename}"
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${OPT_INPUT}"
	)

endfunction(omr_add_hookgen)
