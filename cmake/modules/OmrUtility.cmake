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
#############################################################################

# Include once
if(OMR_UTILITY_)
	return()
endif()
set(OMR_UTILITY_ 1)

include(OmrAssert)

# omr_add_prefix(<output_variable> <prefix> <element>...)
# Given a prefix, and a list of arguments, prefix the list of arguments and
# assign to out: ie, add_prefix(out "-I" "a;b;c") should set out to
# "-Ia;-Ib;-Ic".
function(omr_add_prefix out prefix)
	set(ret "")
	foreach(var IN ITEMS ${ARGN})
		list(APPEND ret "${prefix}${var}")
	endforeach()
	set(${out} ${ret} PARENT_SCOPE)
endfunction(omr_add_prefix)

# omr_trim_whitespace(<output_variable> <string>)
# Trim repeated whitespace from <string> and assign to <output_variable>.
# Does not remove leading/trailing whitespace. For that, use string(STRIP ...)
function(omr_trim_whitespace variable string)
	string(REGEX REPLACE " +" " " result "${string}")
	set(${variable} "${result}" PARENT_SCOPE)
endfunction(omr_trim_whitespace)

# omr_remove_flags(<output_variable> [<flag>...])
# Remove flags from a string.
function(omr_remove_flags variable)
	set(result "${${variable}}")
	foreach(option IN ITEMS ${ARGN})
		string(REGEX REPLACE "(^| )${option}( |$)" " " result "${result}")
	endforeach()
	omr_trim_whitespace(result "${result}")
	set(${variable} "${result}" PARENT_SCOPE)
endfunction(omr_remove_flags)

# omr_join(<output_variable> <item>...)
# takes given items an joins them with <glue>, places result in output variable
function(omr_join glue output)
	string(REGEX REPLACE "(^|[^\\]);" "\\1${glue}" result "${ARGN}")
	set(${output} "${result}" PARENT_SCOPE)
endfunction(omr_join)

# omr_stringify(<output_variable> <item>...)
# Convert items to a string of space-separated items.
function(omr_stringify output)
	omr_join(" " result ${ARGN})
	set(${output} ${result} PARENT_SCOPE)
endfunction(omr_stringify)

# omr_append_flags(<output_variable> [<flag>...])
# Append flags to variable. Flags are space separated.
# <variable> is a string, <flag> is a list.
function(omr_append_flags variable)
	omr_stringify(flags ${ARGN})
	set(${variable} "${${variable}} ${flags}" PARENT_SCOPE)
endfunction(omr_append_flags)

# omr_list_contains(<list> <element> <output>)
# Checks if <element> is in a given named <list>
# sets <output> TRUE/FALSE as appropriate
macro(omr_list_contains lst element output)
	list(FIND ${lst} ${element} _omr_list_contains_idx)
	if(_omr_list_contains_idx EQUAL -1)
		set(${output} FALSE)
	else()
		set(${output} TRUE)
	endif()
	unset(_omr_list_contains_idx)
endmacro(omr_list_contains)

# omr_process_template(<input_file> <output_file> [@ONLY] [ESCAPE_QUOTES])
# processes a template file by first expanding variable references, and then
# evaluating generator expressions. @ONLY and ESCAPE_QUOTES are treated as
# they are for configure_file().
# NOTE: like file(GENERATE ) output is not written until the end cmake evaluation
function(omr_process_template input output)

	set(opts @ONLY ESCAPE_QUOTES)
	cmake_parse_arguments(opt "${opts}" "" "" ${ARGN})

	set(configure_args)
	if(opt_@ONLY)
		list(APPEND configure_args @ONLY)
	endif()
	if(opt_ESCAPE_QUOTES)
		list(APPEND configure_args ESCAPE_QUOTES)
	endif()

	# make input path absolute, treating relative paths relative to current source dir
	get_filename_component(input_abs "${input}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
	omr_assert(SEND_ERROR TEST EXISTS "${input_abs}" MESSAGE "omr_process_template called with non-existant input '${input}'")

	file(READ "${input_abs}" template)
	string(CONFIGURE "${template}" configured_template ${configure_args})
	file(GENERATE OUTPUT "${output}" CONTENT "${configured_template}")
endfunction()

# omr_count_true(<out_var> [<values>...] [VARIABLES <variables>...])
#   count the nuber of <values> and <variables> which evaluate to true
#   return the result in <out_var>
function(omr_count_true out)
	cmake_parse_arguments(opt "" "" "VARIABLES" ${ARGN})
	set(result 0)
	foreach(str IN LISTS opt_UNPARSED_ARGUMENTS)
		if(str)
			set(result "${result}+1")
		endif()
	endforeach()
	foreach(str IN LISTS opt_VARIABLES)
		if("${${str}}")
			set(result "${result}+1")
		endif()
	endforeach()
	math(EXPR result "${result}")
	set("${out}" "${result}" PARENT_SCOPE)
endfunction()
