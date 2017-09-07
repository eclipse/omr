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

if(OMR_HOOKGEN_)
	return()
endif()
set(OMR_HOOKGEN_ 1)

# Process an input hookgen file to generate output source files.
# Usage: omr_add_hookgen(<input> <output>...)
# TODO: currently output in source tree, should be in build tree
# TODO: Dependecy checking is broken, since it checks for output in build tree rather than src
function(omr_add_hookgen input)
	get_filename_component(input_dir "${input}" DIRECTORY)
	add_custom_command(
		OUTPUT ${ARGN}
		COMMAND hookgen ${CMAKE_CURRENT_SOURCE_DIR}/${input}
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${input_dir}
	)
endfunction(omr_add_hookgen)

macro(add_hookgen)
	omr_add_hookgen(${ARGN})
endmacro(add_hookgen)
