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
#############################################################################

include(OmrAssert)

# omr_find_files(<output variable> PATHS <directory>... [FILES <file>...])
#
# Search PATHS for files.
# This function takes an ordered list of directories to search.
# The output is the full path to each found file.
#
# ie. if you have dir1/a.c and dir2/b.c in your directories, and
#
#     FILES  = a.c; b.c
#     PATH = dir1, dir2
#
# then
#
#     OUTPUT = dir1/a.c, dir2/b.c
#
function(omr_find_files output_variable)
	cmake_parse_arguments(arg "" "" "PATHS;FILES" ${ARGN})
	omr_assert(TEST DEFINED arg_PATHS MESSAGE "omr_find_files called without PATHS")

	set(paths "")

	foreach(path ${arg_PATHS})
		if(NOT IS_ABSOLUTE "${path}")
			set(path "${CMAKE_CURRENT_SOURCE_DIR}/${path}")
		endif()
		omr_assert(WARNING TEST EXISTS "${path}" MESSAGE "Could not find path: ${path}")
		list(APPEND paths ${path})
	endforeach()

	set(result "")

	foreach(file ${arg_FILES})
		set(file_found FALSE)
		foreach(path ${paths})
			if(EXISTS "${path}/${file}")
				list(APPEND result "${path}/${file}")
				set(file_found TRUE)
				break()
			endif()
		endforeach()
		omr_assert(WARNING TEST file_found MESSAGE "Could not find file: ${file}")
	endforeach()

	set(${output_variable} "${result}" PARENT_SCOPE)
endfunction()
