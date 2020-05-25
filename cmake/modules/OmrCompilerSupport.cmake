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
if(OMR_COMPILER_SUPPORT_)
	return()
endif()
set(OMR_COMPILER_SUPPORT_ 1)

# This file contains a number of support pieces required to build the compiler
# component.
#
# One of the building blocks of this support is the notion that each compiler
# you may want to build has an associated 'name'. For example, jitbuilder is
# the name of one.

# Tools and helpers:

Find_package(Perl)
include(OmrUtility)

if(NOT PERL_FOUND )
	message(FATAL_ERROR "Perl not found")
endif()

# currently not doing cross compiles, so assume HOST == TARGET
set(TR_TARGET_ARCH ${TR_HOST_ARCH})
set(TR_TARGET_SUBARCH ${TR_HOST_SUBARCH})
set(TR_TARGET_BITS ${TR_HOST_BITS})

set(MASM2GAS_PATH ${omr_SOURCE_DIR}/tools/compiler/scripts/masm2gas.pl)

# Mark a target as consuming the compiler components.
#
# This supplements the compile definitions, options and
# include directories.
#
# call as make_compiler_target(<target> <scope> COMPILER <compiler name>)
function(make_compiler_target TARGET_NAME SCOPE)
	cmake_parse_arguments(TARGET
		"" # optional arguments
		"COMPILER" # single-value arguments
		""
		${ARGN}
		)

	set(COMPILER_DEFINES ${${TARGET_COMPILER}_DEFINES})
	set(COMPILER_ROOT ${${TARGET_COMPILER}_ROOT})
	set(COMPILER_INCLUDES ${${TARGET_COMPILER}_INCLUDES})

	target_include_directories(${TARGET_NAME} BEFORE ${SCOPE}
		${COMPILER_INCLUDES}
	)

	# TODO: Extract into platform specific section.
	target_compile_definitions(${TARGET_NAME} ${SCOPE}
		BITVECTOR_BIT_NUMBERING_MSB
		${TR_COMPILE_DEFINITIONS}
		${COMPILER_DEFINES}
	)

	message(STATUS "Adding ${SCOPE} compiler options to ${TARGET_NAME} for ${TARGET_COMPILER}.")
endfunction(make_compiler_target)

# Filter through the provided list, and rewrite any
# .pasm files to .asm files, and add the .asm file to the list
# in lieu of the .pasm file.
function(pasm2asm_files out_var compiler)
	set(PASM_INCLUDES ${${compiler}_INCLUDES} $ENV{J9SRC}/oti)
	omr_add_prefix(PASM_INCLUDES "-I" ${PASM_INCLUDES})

	set(result "")
	foreach(in_f ${ARGN})
		get_filename_component(extension ${in_f} EXT)
		if(extension STREQUAL ".pasm") # requires preprocessing
			get_filename_component(absolute_in_f ${in_f} ABSOLUTE)

			string(REGEX REPLACE ".pasm$" ".asm" out_f ${in_f})

			set(out_f "${CMAKE_CURRENT_BINARY_DIR}/${out_f}")
			get_filename_component(out_dir ${out_f} DIRECTORY)
			file(MAKE_DIRECTORY ${out_dir})

			add_custom_command(OUTPUT ${out_f}
				COMMAND ${PASM_CMD} ${PASM_FLAGS} ${PASM_INCLUDES} -o ${out_f} ${absolute_in_f}
				DEPENDS ${in_f}
				WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
				COMMENT "Running pasm2asm command on ${in_f} to create ${out_f}"
				VERBATIM
			)

			list(APPEND result ${out_f})
		else()
			list(APPEND result ${in_f}) # no change required - not masm2gas target
		endif()
	endforeach()
	set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# Filter through the provided list, and rewrite any
# .asm files to .s files, and add the .s file to the list
# in lieu of the .asm file.
#
# Inspired by this post: https://cmake.org/pipermail/cmake/2010-June/037733.html
function(masm2gas_asm_files out_var compiler)
	# This portion should be abstracted away, and the ATT_INCLUDES below should be made
	# into target properties to avoid polluting the rest of the info.
	set(MASM2GAS_INCLUDES ${${compiler}_INCLUDES} $ENV{J9SRC}/oti)
	omr_add_prefix(MASM2GAS_INCLUDES "-I" ${MASM2GAS_INCLUDES})
	set(MASM2GAS_FLAGS --64 )
	set(CMAKE_ASM-ATT_INCLUDES ${MASM2GAS_INCLUDES} PARENT_SCOPE)

	set(result "")
	foreach(in_f ${ARGN})
		get_filename_component(extension ${in_f} EXT)
		if(extension STREQUAL ".asm") # Requires masm2gas
			get_filename_component(absolute_in_f ${in_f} ABSOLUTE)

			# From the masm2gas documentation:
			#
			# Output file names follow these rules:
			# foo.asm -> GAS: (outdir)/foo.s DEPEND: (outdir)/foo.d

			# Note: This support doesn't handle dependencies generated by masm2gas unfortunately.
			string(REGEX REPLACE ".asm$" ".s" out_f ${in_f})
			set(out_f "${CMAKE_CURRENT_BINARY_DIR}/${out_f}")
			get_filename_component(out_dir ${out_f} DIRECTORY)
			file(MAKE_DIRECTORY ${out_dir})

			add_custom_command(OUTPUT ${out_f}
				COMMAND ${PERL_EXECUTABLE} ${MASM2GAS_PATH} ${MASM2GAS_FLAGS} ${MASM2GAS_INCLUDES} -o${out_dir}/ ${absolute_in_f}
				DEPENDS ${in_f}
				WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
				COMMENT "Running masm2gas.pl on ${in_f} to create ${out_f}"
				VERBATIM
			)

		list(APPEND result ${out_f})
		else()
			list(APPEND result ${in_f}) # No change required. Not masm2gas target
		endif()
	endforeach()
	set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(spp2s_files out_var compiler)
	# This peculiar pipeline exists to work around some issues that
	# are created by the pickiness of the AIX assembler.
	#
	# Convert an SPP file to an IPP using the pre-processor.
	# Rewrite the IPP file to a .s file using sed.

	# Get the definitions already set in this directory.
	# - A concern would be how this would interact with target_compile_definitions.
	get_property(compile_defs DIRECTORY PROPERTY COMPILE_DEFINITIONS)

	set(SPP_INCLUDES ${${compiler}_INCLUDES})
	set(SPP_DEFINES ${${compiler}_DEFINES} ${compile_defs})

	omr_add_prefix(SPP_INCLUDES "-I" ${SPP_INCLUDES})
	omr_add_prefix(SPP_DEFINES "-D" ${SPP_DEFINES})
	set(result "")
	foreach(in_f ${ARGN})
		get_filename_component(extension ${in_f} EXT)
		if(extension STREQUAL ".spp") # Requires conversion!
			get_filename_component(absolute_in_f ${in_f} ABSOLUTE)

			string(REGEX REPLACE ".spp$" ".ipp" ipp_out_f ${in_f})
			set(ipp_out_f "${CMAKE_CURRENT_BINARY_DIR}/${ipp_out_f}")
			get_filename_component(out_dir ${ipp_out_f} DIRECTORY)
			file(MAKE_DIRECTORY ${out_dir}) # just in case it doesn't exist already

			add_custom_command(OUTPUT ${ipp_out_f}
				COMMAND ${SPP_CMD} ${SPP_FLAGS} ${SPP_DEFINES} ${SPP_INCLUDES} -E -P ${absolute_in_f} > ${ipp_out_f}
				DEPENDS ${in_f}
				WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
				COMMENT "Running preprocessing ${in_f} to create ${ipp_out_f}"
				VERBATIM
			)

			string(REGEX REPLACE ".ipp$" ".s" s_out_f ${ipp_out_f})

			add_custom_command(OUTPUT ${s_out_f}
				COMMAND sed s/\!/\#/g ${ipp_out_f} > ${s_out_f}
				DEPENDS ${ipp_out_f}
				WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
				COMMENT "Replacing comment tokens in ${ipp_out_f} to create ${s_out_f}"
				VERBATIM
			)

			list(APPEND result ${s_out_f})
		else()
			list(APPEND result ${in_f}) # No change required. Not masm2gas target
		endif()
	endforeach()
	set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# Some source files in OMR don't map well into the transforms
# CMake already knows about. This generates a pipeline of custom commands
# to transform these source files into files that CMake _does_ understand.
function(omr_inject_object_modification_targets result compiler_name)
	set(arg ${ARGN})

	# Convert pasm files ot asm files: run this before masm2gas to ensure
	# asm files subsequently get picked up.
	pasm2asm_files(arg ${compiler_name} ${arg})

	# Run masm2gas on contained .asm files.
	masm2gas_asm_files(arg ${compiler_name} ${arg})

	# Convert SPP files to .s files.
	spp2s_files(arg ${compiler_name} ${arg})

	set(${result} ${arg} PARENT_SCOPE)
endfunction(omr_inject_object_modification_targets)

# make_gnu_asm_defines(output_var <define> ...)
# Make output_var a string which will define each <define>
# as an assembler define (rather than a C preprocessor macro).
function(make_gnu_asm_defines output)
	foreach(arg IN LISTS ARGN)
		set(clean_arg)
		string(REGEX REPLACE "^-D" "" clean_arg "${arg}")
		# if clean_arg already is of form FOO=BAR, append it as is
		# otherwise we change it to FOO=1
		if(clean_arg MATCHES "=")
			set(arg_str "${arg_str},--defsym,${clean_arg}")
		else()
			set(arg_str "${arg_str},--defsym,${clean_arg}=1")
		endif()
	endforeach()

	if(OMR_ENV_DATA64)
		set(arg_str ",--64${arg_str}")
	endif()

	# if we actually generated any string
	# i.e. we were passed in any <define> values
	if(arg_str)
		set(${output} "-Wa${arg_str}" PARENT_SCOPE)
	endif()
endfunction(make_gnu_asm_defines)

# Setup the current scope for compiling the Testarossa compiler technology. Used in
# conjunction with make_compiler_target -- only can infect add_directory scope.
macro(set_tr_compile_options)
	omr_append_flags(CMAKE_CXX_FLAGS ${TR_COMPILE_OPTIONS} ${TR_CXX_COMPILE_OPTIONS})
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} PARENT_SCOPE)
	omr_append_flags(CMAKE_C_FLAGS ${TR_COMPILE_OPTIONS} ${TR_C_COMPILE_OPTIONS})
	set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} PARENT_SCOPE)
	# message("[set_tr_compile_options] Set CMAKE_CXX_FLAGS to ${CMAKE_CXX_FLAGS}")
	# message("[set_tr_compile_options] Set CMAKE_C_FLAGS to ${CMAKE_C_FLAGS}")

	set(TR_ASM_FLAGS "")
	foreach(def IN LISTS TR_COMPILE_DEFINITIONS)
		# prepend leading '-D' only if required
		if(def MATCHES "^-D")
			set(TR_ASM_FLAGS "${TR_ASM_FLAGS} ${def}")
			list(APPEND SPP_FLAGS "${def}")
		else()
			set(TR_ASM_FLAGS "${TR_ASM_FLAGS} -D${def}")
			list(APPEND SPP_FLAGS "-D${def}")
		endif()
	endforeach()

	set(SPP_FLAGS ${SPP_FLAGS} PARENT_SCOPE)

	# We need special handling on x86 because we could be GNU or NASM assembler.
	if(OMR_ARCH_X86)
		make_gnu_asm_defines(gnu_defines ${TR_COMPILE_DEFINITIONS})
		set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${gnu_defines}" PARENT_SCOPE)
		set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} ${TR_ASM_FLAGS}" PARENT_SCOPE)
	else()
		set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${TR_ASM_FLAGS}" PARENT_SCOPE)
	endif()
endmacro(set_tr_compile_options)

# Create an OMR Compiler component.
#
# call like this:
# create_omr_compiler_library(
#     NAME     <compilername>
#     OBJECTS  <list of objects to add to the glue>
#     DEFINES  <DEFINES for building library
#     FILTER   <list of default objects to remove from the compiler library>
#     INCLUDES <additional includes for building the library>
#     SHARED   <true if you want a shared object, false if you want a static archive>
#
# FILTER exists to allow compiler subprojects to opt-out of functionality
#        that they would prefer to replace.
function(create_omr_compiler_library)
	cmake_parse_arguments(COMPILER
		"SHARED" # optional arguments
		"NAME;OUTPUT_NAME" # single-value arguments
		"OBJECTS;DEFINES;FILTER;INCLUDES" # multi-value arguments
		${ARGV}
		)

	if(COMPILER_SHARED)
		message(STATUS "Creating shared library for ${COMPILER_NAME}")
		set(LIB_TYPE SHARED)
	else()
		message(STATUS "Creating static library for ${COMPILER_NAME}")
		set(LIB_TYPE STATIC)
	endif()

	set_tr_compile_options()

	get_filename_component(abs_root ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)
	# We use the cache to allow passing information about compiler targets
	# from function to function without having to use lots of temps.
	set(${COMPILER_NAME}_ROOT ${abs_root} CACHE INTERNAL "Root for compiler ${COMPILER_NAME}")
	set(${COMPILER_NAME}_DEFINES ${COMPILER_DEFINES} CACHE INTERNAL "Defines for compiler ${COMPILER_NAME}")
	set(${COMPILER_NAME}_INCLUDES
		${${COMPILER_NAME}_ROOT}/${TR_TARGET_ARCH}/${TR_TARGET_SUBARCH}
		${${COMPILER_NAME}_ROOT}/${TR_TARGET_ARCH}
		${${COMPILER_NAME}_ROOT}
		${omr_SOURCE_DIR}/compiler/${TR_TARGET_ARCH}/${TR_TARGET_SUBARCH}
		${omr_SOURCE_DIR}/compiler/${TR_TARGET_ARCH}
		${omr_SOURCE_DIR}/compiler
		${omr_SOURCE_DIR}
		${COMPILER_INCLUDES}
		CACHE INTERNAL "Include header list for ${COMPILER}"
	)

	# Generate a build name file.
	set(BUILD_NAME_FILE "${CMAKE_BINARY_DIR}/${COMPILER_NAME}Name.cpp")
	add_custom_command(OUTPUT ${BUILD_NAME_FILE}
		COMMAND ${PERL_EXECUTABLE} ${omr_SOURCE_DIR}/tools/compiler/scripts/generateVersion.pl ${COMPILER_NAME} ${BUILD_NAME_FILE}
		VERBATIM
		COMMENT "Generate ${BUILD_NAME_FILE}"
	)

	omr_inject_object_modification_targets(COMPILER_OBJECTS ${COMPILER_NAME} ${COMPILER_OBJECTS})

	# warnings as errors and enhanced warnings currently don't work
	set(OMR_WARNINGS_AS_ERRORS OFF)
	set(OMR_ENHANCED_WARNINGS OFF)
	set(output_name_args "")
	if(COMPILER_OUTPUT_NAME)
		set(output_name_args "OUTPUT_NAME" "${COMPILER_OUTPUT_NAME}")
	endif()
	omr_add_library(${COMPILER_NAME} ${LIB_TYPE}
		${output_name_args}
		${BUILD_NAME_FILE}
		${COMPILER_OBJECTS}
	)

	target_link_libraries(${COMPILER_NAME}
		PUBLIC
			omr_base
	)

	# Grab the list of core compiler objects from the global property.
	# Note: the property is initialized by compiler/CMakeLists.txt.
	get_property(core_compiler_objects GLOBAL PROPERTY OMR_CORE_COMPILER_OBJECTS)

	# Filter out objects requested to be removed by
	# client project (if any):
	foreach(object ${COMPILER_FILTER})
		get_filename_component(abs_filename ${object} ABSOLUTE)
		list(REMOVE_ITEM core_compiler_objects ${abs_filename})
	endforeach()

	omr_inject_object_modification_targets(core_compiler_objects ${COMPILER_NAME} ${core_compiler_objects})

	# Append to the compiler sources list.
	target_sources(${COMPILER_NAME} PRIVATE ${core_compiler_objects})

	# Set include paths and defines.
	make_compiler_target(${COMPILER_NAME} PRIVATE COMPILER ${COMPILER_NAME})
endfunction(create_omr_compiler_library)
