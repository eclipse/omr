###############################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

omr_assert(
	TEST OMR_UTIL_GLUE_TARGET
	MESSAGE "OMR_UTIL_GLUE_TARGET must be set."
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

include(OmrFindFiles)

set(OBJECTS "")
set(VPATH "")

add_tracegen(utilcore.tdf j9utilcore)

omr_add_library(omrutil_obj OBJECT
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9utilcore.c
)

target_include_directories(omrutil_obj
	PUBLIC
		$<TARGET_PROPERTY:omr_base,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(omrutil_obj
	PUBLIC
		$<TARGET_PROPERTY:omr_base,INTERFACE_COMPILE_DEFINITIONS>
)

list(APPEND VPATH .)

if(OMR_ARCH_S390)
	if(OMR_HOST_OS STREQUAL zos)
		if(OMR_ENV_DATA64)
			list(APPEND OBJECTS j9memclrz10_64.s)
			list(APPEND OBJECTS omrget_userExtendedPrivateAreaMemoryType.s)
			list(APPEND OBJECTS zbarrier.s)

			list(APPEND VPATH unix/zos/64)
			target_include_directories(omrutil_obj PRIVATE unix/zos/64)
		else()
			list(APPEND OBJECTS j9memclrz10_31.s)
			list(APPEND OBJECTS omrceetbck.s)
			list(APPEND OBJECTS zbarrier.s)

			list(APPEND VPATH unix/zos/31)
			target_include_directories(omrutil_obj PRIVATE unix/zos/31)
		endif()
		list(APPEND OBJECTS zosversion.c)
		list(APPEND OBJECTS getstoragekey.c)
		list(APPEND OBJECTS getpsw.s)
		list(APPEND OBJECTS omrgetdsa.s)

		list(APPEND VPATH unix/zos)
		target_include_directories(omrutil_obj PRIVATE unix/zos)
	else()
		if(OMR_ENV_DATA64)
			list(APPEND OBJECTS j9memclrz10_64.s)

			list(APPEND VPATH unix/linux/s390/64)
			target_include_directories(omrutil_obj PRIVATE unix/linux/s390/64)
		else()
			list(APPEND OBJECTS j9memclrz10_31.s)

			list(APPEND VPATH unix/linux/s390/31)
			target_include_directories(omrutil_obj PRIVATE unix/linux/s390/31)
		endif()
		list(APPEND OBJECTS archinfo.c)

		list(APPEND VPATH unix/linux/s390)
		target_include_directories(omrutil_obj PRIVATE unix/linux/s390)
	endif()
endif()

if(OMR_ARCH_POWER)
	if(OMR_HOST_OS STREQUAL linux)
		if(OMR_ENV_DATA64)
			if(OMR_ENV_LITTLE_ENDIAN)
				list(APPEND OBJECTS gettoc.s)

				list(APPEND VPATH unix/linux/ppc/64le)
				target_include_directories(omrutil_obj PRIVATE unix/linux/ppc/64le)
			endif()
		endif()
	endif()
	if(NOT OMR_ENV_DATA64)
		list(APPEND OBJECTS cas8help.s)
		if(OMR_HOST_OS STREQUAL linux)
			list(APPEND VPATH unix/linux/ppc/32)
			target_include_directories(omrutil_obj PRIVATE unix/linux/ppc/32)
		else()
			list(APPEND VPATH unix/aix/32)
			target_include_directories(omrutil_obj PRIVATE unix/aix/32)
		endif()
	endif()
endif()

if(OMR_ARCH_RISCV)
	if(OMR_HOST_OS STREQUAL linux)
		list(APPEND OBJECTS cas32helper.s)
		list(APPEND VPATH unix/linux/riscv/32)
		target_include_directories(omrutil_obj PRIVATE unix/linux/riscv/32)

		if(OMR_ENV_DATA64)
			list(APPEND OBJECTS cas64helper.s)
			list(APPEND VPATH unix/linux/riscv/64)
			target_include_directories(omrutil_obj PRIVATE unix/linux/riscv/64)
		endif()
	endif()
endif()

list(APPEND OBJECTS
	AtomicFunctions.cpp
	argscan.c
	detectVMDirectory.c
	gettimebase.c
	j9memclr.c
	poolForPort.c
	primeNumberHelper.c
	ranking.c
	spacesaving.c
	stricmp.c
	threadhelp.c
	thrname_core.c
	utf8decode.c
	utf8encode.c
	xlphelp.c
	xml.c
)

if(OMR_HOST_OS STREQUAL "win")
	target_sources(omrutil_obj
		PRIVATE
			win/omrgetdbghelp.c
	)
endif()

omr_find_files(resolvedPaths
	PATHS ${VPATH}
	FILES ${OBJECTS}
)

target_sources(omrutil_obj PRIVATE ${resolvedPaths})

# This is a bit of a song and dance because CMake
# won't let object libraries link against anything.
# So instead we manually apply the interface properties
target_sources(omrutil_obj PRIVATE $<TARGET_PROPERTY:${OMR_UTIL_GLUE_TARGET},INTERFACE_SOURCES>)
target_include_directories(omrutil_obj PUBLIC $<TARGET_PROPERTY:${OMR_UTIL_GLUE_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)


target_enable_ddr(omrutil_obj)
ddr_set_add_targets(omrddr omrutil_obj)

omr_add_library(omrutil STATIC
	$<TARGET_OBJECTS:omrutil_obj>
)

# Duplicate the export list onto the static library version
get_target_property(util_exports omrutil_obj EXPORTED_SYMBOLS)
omr_add_exports(omrutil ${util_exports})

# Add in the interface include dirs from omrutil_obj to omrutil
target_include_directories(omrutil PUBLIC $<TARGET_PROPERTY:omrutil_obj,INTERFACE_INCLUDE_DIRECTORIES>)

target_link_libraries(omrutil
	PUBLIC
		omr_base
		j9hashtable
)

set_target_properties(omrutil_obj omrutil PROPERTIES FOLDER util)
