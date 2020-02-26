################################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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

compiler_library(control
	${CMAKE_CURRENT_LIST_DIR}/OMROptions.cpp
	${CMAKE_CURRENT_LIST_DIR}/OptimizationPlan.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRRecompilation.cpp
	${CMAKE_CURRENT_LIST_DIR}/CompilationController.cpp
	${CMAKE_CURRENT_LIST_DIR}/CompileMethod.cpp
)

if (OMR_COMPILER_NEW_OPTIONS)
	compiler_library(control
		${CMAKE_CURRENT_LIST_DIR}/OMROptionProcessors.cpp
		${CMAKE_CURRENT_LIST_DIR}/OMRCompilerOptionsManager.cpp
		${CMAKE_CURRENT_LIST_DIR}/OptionsBuilder.cpp
	)

	set(OPTIONS_GENERATOR ${CMAKE_CURRENT_LIST_DIR}/options-gen.py)

	set(OPTIONS_GENERATOR_OUTPUT
		${CMAKE_BINARY_DIR}/compiler/control/Options.inc
		${CMAKE_BINARY_DIR}/compiler/control/OptionInitializerList.inc
		${CMAKE_BINARY_DIR}/compiler/control/OptionTableProperties.inc
		${CMAKE_BINARY_DIR}/compiler/control/OptionTableEntries.inc
		${CMAKE_BINARY_DIR}/compiler/control/OptionCharMap.inc
	)

	add_custom_command(
		OUTPUT ${OPTIONS_GENERATOR_OUTPUT}
		COMMAND echo "running options-gen"
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/compiler/control
		COMMAND ${PYTHON_EXECUTABLE} ${OPTIONS_GENERATOR} -omroptionsdir ${CMAKE_CURRENT_LIST_DIR} -outputdir ${CMAKE_BINARY_DIR}/compiler/control
		DEPENDS ${OPTIONS_GENERATOR}
	)

	add_custom_target(
		generate-options
		DEPENDS ${OPTIONS_GENERATOR_OUTPUT}
	)
endif()