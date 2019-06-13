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

find_program(YACC_EXECUTABLE NAMES yacc DOC "Path to the YACC executable")
mark_as_advanced(YACC_EXECUTABLE)

if(YACC_EXECUTABLE)
	macro(YACC_TARGET_option_extraopts Options)
		set(YACC_TARGET_cmdopt "")
		set(YACC_TARGET_extraopts "${Options}")
		separate_arguments(YACC_TARGET_extraopts)
		list(APPEND YACC_TARGET_cmdopt ${YACC_TARGET_extraopts})
	endmacro()

	macro(YACC_TARGET_option_defines YACCOutput Header)
		if("${Header}" STREQUAL "")
			string(REGEX REPLACE "^(.*)(\\.[^.]*)$" "\\2" _fileext "${YACCOutput}")
			string(REPLACE "c" "h" _fileext ${_fileext})
			string(REGEX REPLACE "^(.*)(\\.[^.]*)$" "\\1${_fileext}" YACC_TARGET_output_header "${YACCOutput}")
			list(APPEND YACC_TARGET_cmdopt "-d")
		else()
			set(YACC_TARGET_output_header "${Header}")

			if(OMR_HOST_OS STREQUAL "zos")
				list(APPEND YACC_TARGET_cmdopt "-D${YACC_TARGET_output_header}")
			else()
				list(APPEND YACC_TARGET_cmdopt "--defines=${YACC_TARGET_output_header}")
			endif()
		endif()
	endmacro()

	macro(YACC_TARGET Name YACCInput YACCOutput)
		set(YACC_TARGET_outputs "${YACCOutput}")
		set(YACC_TARGET_extraoutputs "")
		set(YACC_TARGET_PARAM_OPTIONS)
		set(YACC_TARGET_PARAM_ONE_VALUE_KEYWORDS
			COMPILE_FLAGS
			DEFINES_FILE
		 )

		cmake_parse_arguments(
			YACC_TARGET_ARG
			"${YACC_TARGET_PARAM_OPTIONS}"
			"${YACC_TARGET_PARAM_ONE_VALUE_KEYWORDS}"
			"${LEX_TARGET_MULTI_VALUE_KEYWORDS}"
			${ARGN}
		)

		set(YACC_TARGET_usage "YACC_TARGET(<Name> <Input> <Output> [COMPILE_FLAGS <string>] [DEFINES_FILE <string>]")

		if(NOT "${YACC_TARGET_ARG_UNPARSED_ARGUMENTS}" STREQUAL "")
			message(SEND_ERROR ${YACC_TARGET_usage})
		else()
			YACC_TARGET_option_extraopts("${YACC_TARGET_ARG_COMPILE_FLAGS}")
			YACC_TARGET_option_defines("${YACCOutput}" "${YACC_TARGET_ARG_DEFINES_FILE}")

			list(APPEND YACC_TARGET_outputs "${YACC_TARGET_output_header}")

			add_custom_command(OUTPUT ${YACC_TARGET_outputs}
				COMMAND ${YACC_EXECUTABLE} ${YACC_TARGET_cmdopt} -o ${YACCOutput} ${YACCInput}
				VERBATIM
				DEPENDS ${YACCInput}
				COMMENT "[YACC][${Name}] Building parser with YACC"
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			)

			set(YACC_${Name}_DEFINED TRUE)
			set(YACC_${Name}_INPUT ${YACCInput})
			set(YACC_${Name}_OUTPUTS ${YACC_TARGET_outputs} ${YACC_TARGET_extraoutputs})
			set(YACC_${Name}_COMPILE_FLAGS ${YACC_TARGET_cmdopt})
			set(YACC_${Name}_OUTPUT_SOURCE "${YACCOutput}")
			set(YACC_${Name}_OUTPUT_HEADER "${YACC_TARGET_output_header}")
		endif()
	endmacro()
endif()
