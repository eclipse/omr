###############################################################################
# Copyright (c) 2018, 2019 IBM Corp. and others
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
endif()

execute_process(COMMAND grep -lE "@ddr_(namespace|options):"  ${input_file} TIMEOUT 2 RESULT_VARIABLE rc)
if(rc)
	#input didnt have any ddr directives, so just dump an empty file
	file(WRITE ${output_file} "")
else()
	file(REMOVE ${output_file})

	execute_process(COMMAND awk -f ${AWK_SCRIPT} ${input_file} TIMEOUT 2 OUTPUT_VARIABLE awk_result RESULT_VARIABLE rc )
	if(NOT ${rc})
		file(WRITE "${output_file}" "/* generated file, DO NOT EDIT*/\n")
		if(pre_includes)
			foreach(inc_file IN LISTS pre_includes)
				file(APPEND ${output_file} "#include \"${inc_file}\"\n ")
			endforeach()
		endif()
		file(APPEND ${output_file} "#include \"${input_file}\"\n")

		file(APPEND ${output_file} "${awk_result}")
	endif()
endif()
return(${rc})
