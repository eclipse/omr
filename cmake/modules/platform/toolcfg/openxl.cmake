###############################################################################
# Copyright IBM Corp. and others 2024
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
###############################################################################

if(CMAKE_C_COMPILER_IS_XLCLANG)
	macro(omr_toolconfig_global_setup)
		# For XLClang, remove any usages of -qhalt=e or -qhalt=s provided by default
		# in the CMAKE CXX/C/ASM FLAGS, since xlclang/xlclang++ are not compatible
		# with the e or s options.
		omr_remove_flags(CMAKE_ASM_FLAGS -qhalt=e)
		omr_remove_flags(CMAKE_CXX_FLAGS -qhalt=s)
		omr_remove_flags(CMAKE_C_FLAGS   -qhalt=e)
	endmacro(omr_toolconfig_global_setup)
endif()

if(OMR_HOST_ARCH STREQUAL "ppc")
	set(OMR_C_WARNINGS_AS_ERROR_FLAG -qhalt=w)
	set(OMR_CXX_WARNINGS_AS_ERROR_FLAG -qhalt=w)

	set(OMR_C_ENHANCED_WARNINGS_FLAG )
	set(OMR_CXX_ENHANCED_WARNINGS_FLAG )

	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
		-qxflag=LTOL:LTOL0
	)

	list(APPEND OMR_PLATFORM_CXX_COMPILE_OPTIONS -qlanglvl=extended0x)

	if(OMR_ENV_DATA64)
		list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
			-m64
		)
	else()
		# -march should be there for 32 and 64 C/CXX flags but the C compiler is used for
		# the assembler and it has trouble with some assembly files if it is specified.
		list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
			-march=ppc
		)
	endif()

	# Testarossa build variables. Longer term the distinction between TR and the rest
	# of the OMR code should be heavily reduced. In the mean time, we keep
	# the distinction.

	# TR_COMPILE_OPTIONS are variables appended to CMAKE_{C,CXX}_FLAGS, and so
	# apply to both C and C++ compilations.
	list(APPEND TR_COMPILE_OPTIONS
		-march=pwr7
		-qtls
		-qfuncsect
		-qsuppress=1540-1087:1540-1088:1540-1090:1540-029:1500-029
		-qdebug=nscrep
	)

	if(NOT CMAKE_C_COMPILER_IS_XLCLANG)
		list(APPEND TR_COMPILE_OPTIONS
			-qnotempinc
			-qenum=small
			-qmbcs
		)
	endif()

	# Configure the platform dependent library for multithreading.
	set(OMR_PLATFORM_THREAD_LIBRARY -lpthread)
endif()

if(OMR_OS_AIX)
	list(APPEND OMR_PLATFORM_C_COMPILE_OPTIONS -qlanglvl=extended)
	list(APPEND OMR_PLATFORM_CXX_COMPILE_OPTIONS -qlanglvl=extended0x)

	if(CMAKE_C_COMPILER_IS_XLCLANG)
		list(APPEND OMR_PLATFORM_COMPILE_OPTIONS -qxlcompatmacros)
	else()
		list(APPEND OMR_PLATFORM_COMPILE_OPTIONS -qinfo=pro)
	endif()

	set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lm -liconv -ldl -lperfstat")

	if(OMR_ENV_DATA64)
		set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -q64")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -q64")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -q64")

		set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> -X64 cr <TARGET> <LINK_FLAGS> <OBJECTS>")
		set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> -X64 cr <TARGET> <LINK_FLAGS> <OBJECTS>")
		set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> -X64 <TARGET>")
	endif()

elseif(OMR_OS_LINUX)
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
		-qxflag=selinux
	)
elseif(OMR_OS_ZOS)
	set(OMR_ZOS_COMPILE_ARCHITECTURE "arch10" CACHE STRING "z/OS compile machine architecture" FORCE)
	set(OMR_ZOS_COMPILE_TARGET "ZOSV2R4" CACHE STRING "z/OS compile target operating system" FORCE)
	set(OMR_ZOS_COMPILE_TUNE "12" CACHE STRING "z/OS compile machine architecture tuning" FORCE)
	set(OMR_ZOS_LINK_COMPAT "ZOSV2R4" CACHE STRING "z/OS link compatible operating system" FORCE)
	set(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "--shared")
	set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "--shared")

	set(CMAKE_ASM_FLAGS "-fno-integrated-as")
	string(APPEND CMAKE_ASM_FLAGS " \"-Wa,-mgoff\"")
	string(APPEND CMAKE_ASM_FLAGS " \"-Wa,-mSYSPARM(BIT64)\"")

	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
		"-fstrict-aliasing"
		"-mzos-target=${OMR_ZOS_COMPILE_TARGET}"
		"-m64"
	)

	list(APPEND OMR_PLATFORM_C_COMPILE_OPTIONS
		-march=${OMR_ZOS_COMPILE_ARCHITECTURE}
	)

	list(APPEND OMR_PLATFORM_CXX_COMPILE_OPTIONS
		-march=${OMR_ZOS_COMPILE_ARCHITECTURE}
		"-std=c++14"
		-fasm
	)

	list(APPEND OMR_PLATFORM_SHARED_COMPILE_OPTIONS
		-fvisibility=default
	)

	if(OMR_ENV_DATA64)
		list(APPEND OMR_PLATFORM_DEFINITIONS
			-DJ9ZOS39064
		)
	else()
		list(APPEND OMR_PLATFORM_DEFINITIONS
			-D_LARGE_FILES
		)
	endif()

	# Testarossa build variables. Longer term the distinction between TR and the rest
	# of the OMR code should be heavily reduced. In the mean time, we keep
	# the distinction.

	# TR_COMPILE_OPTIONS are variables appended to CMAKE_{C,CXX}_FLAGS, and so
	# apply to both C and C++ compilations.
	list(APPEND TR_COMPILE_OPTIONS
		-DYYLMAX=1000
		-Wa,asa
	)

	list(APPEND TR_CXX_COMPILE_OPTIONS
		-Wc,EXH
		-qhaltonmsg=CCN6102
		-qnocsect
	)

	# Configure the platform dependent library for multithreading.
	set(OMR_PLATFORM_THREAD_LIBRARY "")
endif()

set(SPP_CMD ${CMAKE_C_COMPILER})

if(CMAKE_C_COMPILER_IS_XLCLANG)
	# The -P option doesn't sit well with XLClang, so it's not included. It causes:
	# "ld: 0711-317 ERROR: Undefined symbol: <SYMBOL>" when libj9jit29.so is getting linked.
	set(SPP_FLAGS -E)
else()
	set(SPP_FLAGS -E -P)
endif()

if(OMR_OS_ZOS)
	function(_omr_toolchain_process_exports TARGET_NAME)
		# Any type of target which says it has exports should get the DLL, and EXPORTALL
		# compile flags.
		# Open XL equivalent has been added below.
		target_compile_options(${TARGET_NAME}
			PRIVATE
				-fvisibility=default
		)

		get_target_property(target_type ${TARGET_NAME} TYPE)
		if(NOT target_type STREQUAL "SHARED_LIBRARY")
			return()
		endif()
		add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
			COMMAND "${CMAKE_COMMAND}"
				"-DLIBRARY_FILE_NAME=$<TARGET_FILE_NAME:${TARGET_NAME}>"
				"-DRUNTIME_DIR=$<TARGET_FILE_DIR:${TARGET_NAME}>"
				"-DARCHIVE_DIR=$<TARGET_PROPERTY:${TARGET_NAME},ARCHIVE_OUTPUT_DIRECTORY>"
				-P "${omr_SOURCE_DIR}/cmake/modules/platform/toolcfg/zos_rename_exports.cmake"
		)
	endfunction()
else()
	function(_omr_toolchain_process_exports TARGET_NAME)
		get_target_property(target_type ${TARGET_NAME} TYPE)
		if(NOT target_type STREQUAL "SHARED_LIBRARY")
			return()
		endif()

		set(exp_file "$<TARGET_PROPERTY:${TARGET_NAME},BINARY_DIR>/${TARGET_NAME}.exp")
		omr_process_template(
			"${omr_SOURCE_DIR}/cmake/modules/platform/toolcfg/xlc_exports.exp.in"
			"${exp_file}"
		)
		set_property(TARGET ${TARGET_NAME} APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-bE:${TARGET_NAME}.exp")
	endfunction()

	function(_omr_toolchain_separate_debug_symbols tgt)
		set(exe_file "$<TARGET_FILE:${tgt}>")
		omr_get_target_output_genex(${tgt} output_name)
		set(dbg_file "${output_name}${OMR_DEBUG_INFO_OUTPUT_EXTENSION}")
		add_custom_command(
			TARGET "${tgt}"
			POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy ${exe_file} ${dbg_file}
			COMMAND "${CMAKE_STRIP}" -X32_64 -t ${exe_file}
		)
		set_target_properties(${tgt} PROPERTIES OMR_DEBUG_FILE "${dbg_file}")
	endfunction()
endif()
