###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
# 
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
# 
# This Source Code is also Distributed under one or more Secondary Licenses,
# as those terms are defined by the Eclipse Public License, v. 2.0: GNU
# General Public License, version 2 with the GNU Classpath Exception [1]
# and GNU General Public License, version 2 with the OpenJDK Assembly
# Exception [2].
# 
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
# 
# Contributors:
#   Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################

if (NOT CMAKE_CROSSCOMPILING)

	add_subdirectory(hookgen)
	add_subdirectory(tracemerge)
	add_subdirectory(tracegen)

	export(TARGETS hookgen tracemerge tracegen FILE "ImportTools.cmake")

else(NOT CMAKE_CROSSCOMPILING)

	set(OMR_TOOLS_IMPORTFILE "IMPORTFILE-NOTFOUND" CACHE FILEPATH
		"Point it to the ImportTools.cmake file of a native build")

	#Logically we should be importing the file here, however,
	#buried in the cmake docs, it says imported targets are directory scoped
	#thus they would be useless if imported here
	#include(${OMR_TOOLS_IMPORTFILE})

endif(NOT CMAKE_CROSSCOMPILING)


#TODO: currently output in source tree, should be in build tree
#TODO: Dependecy checking is broken, since it checks for output in build tree rather than src
function(add_hookgen input)
	get_filename_component(input_dir "${input}" DIRECTORY)
	#message("Hookgen, Output = ${ARGN}")
	#message("Hookgen, depends = ${CMAKE_CURRENT_SOURCE_DIR}/${input}")
	add_custom_command(
		OUTPUT ${ARGN}
		COMMAND hookgen ${CMAKE_CURRENT_SOURCE_DIR}/${input}
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${input_dir}
	)
endfunction()


#TODO: pehaps should detect output by searching for "executable=" line
#takes extra optional argument name to override output filename
function(add_tracegen input)
	get_filename_component(input_dir "${input}" DIRECTORY)

	if(ARGV1)
		set(base_name "${ARGV1}")
	else()
		get_filename_component(base_name "${input}" NAME_WE)
	endif()
	#file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${input_dir}/ut_${base_name}" generated_filename)
	file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/ut_${base_name}" generated_filename)

	add_custom_command(
		OUTPUT "${generated_filename}.c" "${generated_filename}.h" "${generated_filename}.pdat"
		COMMAND $<TARGET_FILE:tracegen> -w2cd -treatWarningAsError -generatecfiles -threshold 1 -file ${CMAKE_CURRENT_SOURCE_DIR}/${input}
		DEPENDS ${input}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
endfunction()

#TODO we still dont do tracemerge properly
