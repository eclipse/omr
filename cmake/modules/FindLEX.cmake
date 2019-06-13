###############################################################################
# Copyright (c) 2019, 2019 IBM Corp. and others
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

find_program(LEX_EXECUTABLE NAMES lex DOC "Path to the lex executable")
mark_as_advanced(LEX_EXECUTABLE)

if(LEX_EXECUTABLE)
	macro(LEX_TARGET Name Input Output)
		set(LEX_TARGET_outputs "${Output}")
		set(LEX_EXECUTABLE_opts "")
		set(LEX_TARGET_PARAM_OPTIONS)
		set(LEX_TARGET_PARAM_ONE_VALUE_KEYWORDS
			COMPILE_FLAGS
		)
		set(LEX_TARGET_PARAM_MULTI_VALUE_KEYWORDS)

		cmake_parse_arguments(
			LEX_TARGET_ARG
			"${LEX_TARGET_PARAM_OPTIONS}"
			"${LEX_TARGET_PARAM_ONE_VALUE_KEYWORDS}"
			"${LEX_TARGET_MULTI_VALUE_KEYWORDS}"
			${ARGN}
		)

		set(LEX_TARGET_usage "LEX_TARGET(<Name> <Input> <Output> [COMPILE_FLAGS <string>]")

		if(NOT "${LEX_TARGET_ARG_UNPARSED_ARGUMENTS}" STREQUAL "")
			message(SEND_ERROR ${LEX_TARGET_usage})
		else()
			if(NOT "${LEX_TARGET_ARG_COMPILE_FLAGS}" STREQUAL "")
				set(LEX_EXECUTABLE_opts "${LEX_TARGET_ARG_COMPILE_FLAGS}")
				separate_arguments(LEX_EXECUTABLE_opts)
			endif()

			add_custom_command(OUTPUT ${LEX_TARGET_outputs}
				COMMAND ${LEX_EXECUTABLE} ${LEX_EXECUTABLE_opts} -o${Output} ${Input}
				VERBATIM
				DEPENDS ${Input}
				COMMENT "[LEX][${Name}] Building scanner with LEX ${LEX_VERSION}"
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			)

			set(LEX_${Name}_DEFINED TRUE)
			set(LEX_${Name}_OUTPUTS ${Output})
			set(LEX_${Name}_INPUT ${Input})
			set(LEX_${Name}_COMPILE_FLAGS ${LEX_EXECUTABLE_opts})
		endif()
	endmacro()

	macro(ADD_LEX_YACC_DEPENDENCY LEXTarget YACCTarget)
		if(NOT LEX_${LEXTarget}_OUTPUTS)
			message(SEND_ERROR "LEX target `${LEXTarget}' does not exist.")
		endif()

		if(NOT YACC_${YACCTarget}_OUTPUT_HEADER)
			message(SEND_ERROR "YACC target `${YACCTarget}' does not exist.")
		endif()

		set_source_files_properties(${LEX_${LEXTarget}_OUTPUTS}
			PROPERTIES OBJECT_DEPENDS ${YACC_${YACCTarget}_OUTPUT_HEADER})
	endmacro()
endif()
