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

if(OMR_TRACEGEN_)
	return()
endif()
set(OMR_TRACEGEN_ 1)

add_custom_target(run_tracegen)
set_property(TARGET run_tracegen PROPERTY FOLDER tracegen)

# Setup a default trace root if one has not alreay been set.
if(NOT DEFINED OMR_TRACE_ROOT)
	set(OMR_TRACE_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
endif()
message(STATUS "OMR: trace root is '${OMR_TRACE_ROOT}'")

# Process an <input> tracegen file to produce trace headers, sources, and pdat files.
# Usage: omr_add_tracegen(<input> [<output>])
# By default, <output> is derived from the base name of <input>.
# tracegen will produce:
# - ut_<output>.h
# - ut_<output>.c
# - ut_<output>.pdat
# TODO: perhaps should detect output by searching for "executable=" line
# takes extra optional argument name to override output filename
function(omr_add_tracegen input)
	get_filename_component(input_dir "${input}" DIRECTORY)

	if(ARGV1)
		set(base_name "${ARGV1}")
	else()
		get_filename_component(base_name "${input}" NAME_WE)
	endif()

	get_property(all_trace_modules GLOBAL PROPERTY OMR_TRACE_MODULES)
	omr_list_contains(all_trace_modules "${base_name}" duplicate_module)
	if(duplicate_module)
		message(FATAL_ERROR "Trace module names are required to be unique. Found duplicate module '${base_name}'")
	endif()
	unset(duplicate_module)

	file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/ut_${base_name}" generated_filename)
	set_property(GLOBAL APPEND PROPERTY OMR_TRACE_MODULES ${base_name})
	set_property(TARGET run_tracegen APPEND PROPERTY OMR_TRACE_PDATS "${generated_filename}.pdat")

	add_custom_command(
		OUTPUT "${generated_filename}.c" "${generated_filename}.h" "${generated_filename}.pdat"
		COMMAND ${OMR_EXE_LAUNCHER} $<TARGET_FILE:tracegen> -w2cd -treatWarningAsError -generatecfiles -threshold 1 -file ${CMAKE_CURRENT_SOURCE_DIR}/${input}
		DEPENDS ${input} tracegen # adding tracegen as a dependency should be automatic, but for some reason doesn't happen on ninja generators
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
	add_custom_target("trc_${base_name}" DEPENDS "${generated_filename}.c")
	add_dependencies(run_tracegen "trc_${base_name}")
	set_property(TARGET "trc_${base_name}" PROPERTY FOLDER tracegen)
endfunction(omr_add_tracegen)

macro(add_tracegen)
	omr_add_tracegen(${ARGN})
endmacro(add_tracegen)

# Define a target named 'run_tracegen' which forces generation of all tracegen files (i.e. by depending on run_tracegen)
# However this is really only a build order dependency in cmake. In order to have proper dependency tracking
# based on the output .pdat files we use the generator expression.
add_custom_command(OUTPUT tracemerge.stamp
	COMMAND ${OMR_EXE_LAUNCHER} $<TARGET_FILE:tracemerge> -majorversion 5 -minorversion 1 -root ${OMR_TRACE_ROOT}
	COMMAND ${CMAKE_COMMAND} -E touch tracemerge.stamp
	DEPENDS run_tracegen $<TARGET_PROPERTY:run_tracegen,OMR_TRACE_PDATS>
	WORKING_DIRECTORY ${OMR_TRACE_ROOT}
)
add_custom_target(run_tracemerge ALL
	DEPENDS tracemerge.stamp
)
set_property(TARGET run_tracemerge PROPERTY FOLDER tracegen)
