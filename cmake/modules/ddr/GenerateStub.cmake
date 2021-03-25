###############################################################################
# Copyright (c) 2018, 2021 IBM Corp. and others
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

if(NOT input_file)
	message(FATAL_ERROR "No input file")
elseif(NOT EXISTS "${input_file}")
	message(FATAL_ERROR "Input file '${input_file}' does not exist")
endif()

macro(convert_path output filename)
	if(PATH_TOOL)
		execute_process(
			COMMAND ${PATH_TOOL} "${filename}"
			OUTPUT_VARIABLE _converted_path
			RESULT_VARIABLE _convert_rc
		)
		if(NOT _convert_rc EQUAL 0)
			message(FATAL_ERROR "Error converting path ${filename}")
		endif()

		# remove excess whitespace and save into result variable
		string(STRIP "${_converted_path}" ${output})
	else()
		# no defined tool to convert path names. Do nothing
		set(${output} "${filename}")
	endif()
endmacro()

execute_process(COMMAND grep -E -q "@ddr_(namespace|options):" ${input_file} RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
	# input didn't have any DDR directives, so just dump an empty file
	file(WRITE ${output_file} "")
	set(rc 0)
else()
	execute_process(COMMAND awk -f ${AWK_SCRIPT} ${input_file} OUTPUT_VARIABLE awk_result RESULT_VARIABLE rc)

	if(NOT rc EQUAL 0)
		message(FATAL_ERROR "GenerateStub: Invoking awk script failed (${rc})")
	else()
		file(WRITE "${output_file}" "/* generated file, DO NOT EDIT */\n")
		if(pre_includes)
			foreach(inc_file IN LISTS pre_includes)
				convert_path(native_inc_file "${inc_file}")
				file(APPEND ${output_file} "#include \"${native_inc_file}\"\n")
			endforeach()
		endif()
		convert_path(native_input_file "${input_file}")
		file(APPEND ${output_file} "#include \"${native_input_file}\"\n")
		file(APPEND ${output_file} "${awk_result}")
	endif()
endif()

return(${rc})
