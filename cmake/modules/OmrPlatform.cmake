###############################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
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

if(OMR_PLATFORM_)
	return()
endif()
set(OMR_PLATFORM_ 1)

include(OmrAssert)
include(OmrDetectSystemInformation)
include(OmrUtility)

###
### Platform flags
### TODO: arch flags. Defaulting to x86-64

omr_detect_system_information()

if(NOT OMR_HOST_OS STREQUAL "zos")
	enable_language(ASM)
endif()

# Pickup OS info
include(platform/os/${OMR_HOST_OS})

# Pickup Arch Info
include(platform/arch/${OMR_HOST_ARCH})

# Pickup toolconfig based on platform
include(platform/toolcfg/${OMR_TOOLCONFIG})

# Verify toolconfig!
include(platform/toolcfg/verify)

macro(omr_platform_global_setup)

	omr_assert(WARNING TEST NOT OMR_PLATFORM_GLOBALLY_INITIALIZED
		MESSAGE "omr_platform_global_setup called twice."
	)

	set(OMR_PLATFORM_GLOBALLY_INITIALIZED 1)


	omr_append_flags(CMAKE_C_FLAGS ${OMR_PLATFORM_COMPILE_OPTIONS})
	omr_append_flags(CMAKE_CXX_FLAGS ${OMR_PLATFORM_COMPILE_OPTIONS})
	omr_append_flags(CMAKE_ASM_FLAGS ${OMR_PLATFORM_COMPILE_OPTIONS})

	omr_append_flags(CMAKE_C_FLAGS
		${OMR_PLATFORM_C_COMPILE_OPTIONS}
	)

	omr_append_flags(CMAKE_CXX_FLAGS
		${OMR_PLATFORM_CXX_COMPILE_OPTIONS}
	)

	omr_append_flags(CMAKE_EXE_LINKER_FLAGS
		${OMR_PLATFORM_LINKER_OPTIONS}
		${OMR_PLATFORM_EXE_LINKER_OPTIONS}
	)

	omr_append_flags(CMAKE_SHARED_LINKER_FLAGS
		${OMR_PLATFORM_LINKER_OPTIONS}
		${OMR_PLATFORM_SHARED_LINKER_OPTIONS}
	)

	omr_append_flags(CMAKE_STATIC_LINKER_FLAGS
		${OMR_PLATFORM_LINKER_OPTIONS}
		${OMR_PLATFORM_STATIC_LINKER_OPTIONS}
	)

	include_directories(
		${OMR_PLATFORM_INCLUDE_DIRECTORIES}
	)

	add_definitions(
		${OMR_PLATFORM_DEFINITIONS}
	)

	link_libraries(
		${OMR_PLATFORM_LIBRARIES}
	)

	# If the OS requires global setup, do it here.
	if(COMMAND omr_arch_global_setup)
		omr_arch_global_setup()
	endif()

	# If the OS requires global setup, do it here.
	if(COMMAND omr_os_global_setup)
		omr_os_global_setup()
	endif()

	# And now the toolconfig setup
	if(COMMAND omr_toolconfig_global_setup)
		omr_toolconfig_global_setup()
	endif()
endmacro()

# omr_process_exports(<target> [symbol]...)
# performs appropriate processing to export symbols from a target.
# Note: function is internaly guarded, so its safe to call multiple times
function(omr_process_exports target)
	omr_assert(FATAL_ERROR TEST TARGET ${target} MESSAGE "omr_process_exports called on invalid target '${target}'")

	# Check if we have already processed the exports
	get_target_property(has_processed ${target} OMR_EXPORTS_PROCESSED)

	if((NOT has_processed) AND (COMMAND _omr_toolchain_process_exports))
		_omr_toolchain_process_exports(${target})
	endif()

	set_target_properties(${target} PROPERTIES OMR_EXPORTS_PROCESSED TRUE)
endfunction()

# omr_add_exports(<target>)
# Exports given symbols from a given target, and calls omr_process_exports
function(omr_add_exports target)
	omr_assert(FATAL_ERROR TEST TARGET ${target} MESSAGE "omr_add_exports called on invalid target '${target}'")
	set_property(TARGET ${target} APPEND PROPERTY EXPORTED_SYMBOLS ${ARGN})
	omr_process_exports(${target})
endfunction()

###
### Flags we aren't using
###

# TODO: SPEC

# TODO: OMR_HOST_ARCH
# TODO: OMR_TARGET_DATASIZE
# TODO: OMR_TOOLCHAIN
# TODO: OMR_CROSS_CONFIGURE
# TODO: OMR_RTTI
