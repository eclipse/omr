###############################################################################
# Copyright (c) 2018, 2020 IBM Corp. and others
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

if(_OMR_DDR_SUPPORT)
	return()
endif()
set(_OMR_DDR_SUPPORT 1)

include(OmrAssert)
include(OmrUtility)
include(ExternalProject)

# Make sure we have performed platform detection
include(OmrPlatform)

set(OMR_MODULES_DIR ${CMAKE_CURRENT_LIST_DIR})
set(DDR_INFO_DIR "${CMAKE_BINARY_DIR}/ddr_info")


function(make_ddr_set set_name)
	set(DDR_TARGET_NAME "${set_name}")
	set(DDR_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/${DDR_TARGET_NAME}")
	set(DDR_MACRO_INPUTS_FILE "${DDR_BIN_DIR}/macros.list")
	set(DDR_TOOLS_EXPORT "${omr_BINARY_DIR}/ddr/tools/DDRTools.cmake")
	set(DDR_CONFIG_STAMP "${DDR_BIN_DIR}/config.stamp")

	# if DDR is not enabled, just skip
	# Also skip if we are on a multi config generator since it is unsupported at the moment
	if(OMR_MULTI_CONFIG OR (NOT OMR_DDR))
		# create a dummy target to avoid potential errors if ddr disabled
		add_custom_target("${DDR_TARGET_NAME}")
		return()
	endif()

	add_custom_command(
		OUTPUT "${DDR_CONFIG_STAMP}"
		COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
		COMMAND "${CMAKE_COMMAND}" -E touch "${DDR_CONFIG_STAMP}"
		WORKING_DIRECTORY "${DDR_BIN_DIR}"
	)
	add_custom_target(${DDR_TARGET_NAME}
		DEPENDS "${DDR_CONFIG_STAMP}"
		COMMAND ${CMAKE_COMMAND} --build "${DDR_BIN_DIR}"
	)
	set_property(TARGET "${DDR_TARGET_NAME}" PROPERTY DDR_BIN_DIR "${DDR_BIN_DIR}")
	set_property(TARGET "${DDR_TARGET_NAME}" PROPERTY DDR_SET TRUE)

	file(READ ${OMR_MODULES_DIR}/ddr/DDRSetStub.cmake.in cmakelist_template)
	string(CONFIGURE "${cmakelist_template}" cmakelist_template @ONLY)
	file(GENERATE OUTPUT ${DDR_BIN_DIR}/CMakeLists.txt CONTENT "${cmakelist_template}")

	# Note: DDR sets have themselves as targets to process.
	# This is so that you can process misc headers which don't logically belong to any other target.
	target_enable_ddr(${DDR_TARGET_NAME})
	set_property(TARGET "${DDR_TARGET_NAME}" APPEND PROPERTY DDR_TARGETS "${DDR_TARGET_NAME}")
endfunction(make_ddr_set)

function(ddr_set_add_targets ddr_set)
	if(OMR_MULTI_CONFIG OR (NOT OMR_DDR))
		return()
	endif()

	omr_assert(FATAL_ERROR TEST TARGET "${ddr_set}" MESSAGE "ddrset_add_targets called on non-existent ddr-set ${ddr_set}")
	get_target_property(is_ddrset "${ddr_set}" DDR_SET)
	omr_assert(FATAL_ERROR TEST is_ddrset MESSAGE "ddrset_add_targets called on ddr-set ${ddr_set}, which is not a ddr set")
	foreach(tgt IN LISTS ARGN)
		# Check if the target we are adding is itself a DDR set.
		get_target_property(tgt_is_ddr_set ${tgt} DDR_SET)
		if(tgt_is_ddr_set)
			set_property(TARGET ${ddr_set} APPEND PROPERTY DDR_SUBSETS ${tgt})
		else()
			set_property(TARGET ${ddr_set} APPEND PROPERTY DDR_TARGETS ${tgt})
		endif()
		add_dependencies(${ddr_set} ${tgt})
	endforeach()
endfunction(ddr_set_add_targets)

function(target_enable_ddr tgt)
	if(OMR_MULTI_CONFIG OR (NOT OMR_DDR))
		return()
	endif()

	omr_assert(FATAL_ERROR TEST TARGET ${tgt} MESSAGE "target_enable_ddr called on non-existent target ${tgt}")

	set(options
		EARLY_SOURCE_EVAL
		GLOB_HEADERS
		GLOB_HEADERS_RECURSIVE
		NO_DEBUG_INFO
	)
	set(oneValueArgs "")
	set(multiValueArgs "")

	# Clear variables.
	foreach(opt_name IN LISTS options oneValueArgs multiValueArgs)
		set(opt_${opt_name} "")
	endforeach()

	cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
	omr_assert(FATAL_ERROR TEST NOT opt_UNPARSED_ARGUMENTS MESSAGE "target_enable_ddr: unrecognized options ${opt_UNPARSED_ARGUMENTS}")

	get_target_property(target_type "${tgt}" TYPE)
	if(target_type MATCHES "INTERFACE_LIBRARY")
		message(FATAL_ERROR "Cannot call enable_ddr on interface libraries")
	endif()

	if(opt_EARLY_SOURCE_EVAL)
		set(source_property "DDR_EVAL_SOURCE")
		get_target_property(sources "${tgt}" "SOURCES")
		string(GENEX_STRIP "${sources}" cleaned_sources)
		set_target_properties("${tgt}" PROPERTIES ${source_property} "${cleaned_sources}")
	else()
		set(source_property "SOURCES")
	endif()
	omr_join("\n" MAGIC_TEMPLATE
		"SOURCE_DIR"
		"$<TARGET_PROPERTY:${tgt},SOURCE_DIR>"
		"INCLUDE_PATH"
		"$<JOIN:$<TARGET_PROPERTY:${tgt},INCLUDE_DIRECTORIES>,\n>"
		"DEFINES"
		"$<JOIN:$<TARGET_PROPERTY:${tgt},COMPILE_DEFINITIONS>,\n>"
		"SOURCES"
		"$<JOIN:$<TARGET_PROPERTY:${tgt},${source_property}>,\n>"
		"HEADERS"
		"$<JOIN:$<TARGET_PROPERTY:${tgt},DDR_HEADERS>,\n>"
		"PREINCLUDES"
		"$<JOIN:$<TARGET_PROPERTY:${tgt},DDR_PREINCLUDES>,\n>"
	)
	if((target_type MATCHES "EXECUTABLE|SHARED_LIBRARY") AND (NOT opt_NO_DEBUG_INFO))
		# Ensure that debug info has been processed
		omr_process_split_debug(${tgt})
		get_target_property(debug_file "${tgt}" OMR_DEBUG_FILE)

		# if no value is present, assume the debug info lives in the target file
		if(NOT debug_file)
			set(debug_file "$<TARGET_FILE:${tgt}>")
		else()
			# For non flattened debug on osx, the debug "file" is actually a folder
			if(OMR_OS_OSX AND NOT OMR_FLATTEN_DEBUG_INFO)
				set(debug_file "${debug_file}/Contents/Resources/DWARF/$<TARGET_FILE_NAME:${tgt}>")
			endif()
		endif()

		set(MAGIC_TEMPLATE "OUTPUT_FILE\n${debug_file}\n${MAGIC_TEMPLATE}")
	endif()

	file(GENERATE OUTPUT "${DDR_INFO_DIR}/targets/${tgt}.txt" CONTENT "${MAGIC_TEMPLATE}\n")

	if(opt_GLOB_HEADERS OR opt_GLOB_HEADERS_RECURSIVE)
		get_target_property(source_dir ${tgt} SOURCE_DIR)
		if(opt_GLOB_HEADERS_RECURSIVE)
			set(glob GLOB_RECURSE)
		else()
			set(glob GLOB)
		endif()
		file(${glob} c_headers "${source_dir}/*.h")
		file(${glob} cpp_headers "${source_dir}/*.hpp")
		ddr_add_headers(${tgt}
			${c_headers}
			${cpp_headers}
		)
	endif()
endfunction(target_enable_ddr)

function(ddr_add_headers tgt)
	if(NOT OMR_DDR)
		return()
	endif()
	set_property(TARGET ${tgt} APPEND PROPERTY DDR_HEADERS ${ARGN})
endfunction(ddr_add_headers)
