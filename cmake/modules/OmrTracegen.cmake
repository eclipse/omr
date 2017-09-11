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

if(OMR_TRACEGEN_)
	return()
endif()
set(OMR_TRACEGEN 1)

# Process an <input> tracegen file to produce trace headers, sources, and pdat files.
# Usage: omr_add_tracegen(<input> [<output>])
# By default, <output> is derived from the base name of <input>.
# tracegen will produce:
#   ut_<output>.h
#   ut_<output>.c
#   ut_<output>.pdat
#TODO: pehaps should detect output by searching for "executable=" line
#takes extra optional argument name to override output filename
function(omr_add_tracegen input)
	get_filename_component(input_dir "${input}" DIRECTORY)

	if(ARGV1)
		set(base_name "${ARGV1}")
	else()
		get_filename_component(base_name "${input}" NAME_WE)
	endif()
	file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/ut_${base_name}" generated_filename)

	add_custom_command(
		OUTPUT "${generated_filename}.c" "${generated_filename}.h" "${generated_filename}.pdat"
		COMMAND tracegen -w2cd -treatWarningAsError -generatecfiles -threshold 1 -file ${CMAKE_CURRENT_SOURCE_DIR}/${input}
		DEPENDS ${input}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
endfunction(omr_add_tracegen)

macro(add_tracegen)
	omr_add_tracegen(${ARGN})
endmacro(add_tracegen)
