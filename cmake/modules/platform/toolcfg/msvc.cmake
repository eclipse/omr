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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#############################################################################

include(OmrUtility)

set(OMR_WARNING_AS_ERROR_FLAG /WX)

set(OMR_ENHANCED_WARNING_FLAG /W3)

list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
	/GR-    # Disable RTTI
	/Zm400  # Precompiled header memory allocation limit
	/wd4577 # Disable warning: Specifying noexcept when exceptions are disabled
	/wd4091 # Disable warning: Caused by broken windows SDK, see also https://connect.microsoft.com/VisualStudio/feedback/details/1302025/warning-c4091-in-sdk-7-1a-shlobj-h-1051-dbghelp-h-1054-3056
)

if(OMR_ENV_DATA64)
	set(TARGET_MACHINE "AMD64")
else()
	set(TARGET_MACHINE "i386")
endif()

list(APPEND OMR_PLATFORM_LINKER_OPTIONS
	-subsystem:console
	-machine:${TARGET_MACHINE}
	/NOLOGO # Don't print the msvc logo
)

list(APPEND OMR_PLATFORM_EXE_LINKER_OPTIONS
	/LARGEADDRESSAWARE
	/INCREMENTAL:NO
	wsetargv.obj
)

list(APPEND OMR_PLATFORM_SHARED_LINKER_OPTIONS
	/INCREMENTAL:NO
)

if(OMR_ENV_DATA64)
	list(APPEND OMR_PLATFORM_SHARED_LINKER_OPTIONS
		-entry:_DllMainCRTStartup
		# TODO: makefile has this but it seems to break linker
		# /NODEFAULTLIB:MSVCRTD
	)
elseif(OMR_ENV_DATA32)
	list(APPEND OMR_PLATFORM_LINKER_OPTIONS
		/SAFESEH
	)
	list(APPEND OMR_PLATFORM_SHARED_LINKER_OPTIONS
		-entry:_DllMainCRTStartup@12
	)
endif(OMR_ENV_DATA64)

macro(omr_toolconfig_global_setup)
	# Make sure we are building without incremental linking
	omr_remove_flags(CMAKE_EXE_LINKER_FLAGS    /INCREMENTAL)
	omr_remove_flags(CMAKE_SHARED_LINKER_FLAGS /INCREMENTAL)

	foreach(build_type IN LISTS CMAKE_CONFIGURATION_TYPES)
		string(TOUPPER ${build_type} build_type)
		omr_remove_flags(CMAKE_EXE_LINKER_FLAGS_${build_type}    /INCREMENTAL)
		omr_remove_flags(CMAKE_SHARED_LINKER_FLAGS_${build_type} /INCREMENTAL)
	endforeach()

	# Strip out exception handling flags (added by default by cmake)
	omr_remove_flags(CMAKE_CXX_FLAGS /EHsc /GR)

	# FIXME: disable several warnings while compiling with CLang.
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		omr_append_flags(CMAKE_C_FLAGS -Wno-error=unused-command-line-argument -Wno-error=comment -Wno-error=deprecated)
		omr_append_flags(CMAKE_CXX_FLAGS -Wno-error=unused-command-line-argument -Wno-error=comment -Wno-error=deprecated)
	endif()

	# Hack up output dir to fix dll dependency issues on windows
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endmacro(omr_toolconfig_global_setup)
