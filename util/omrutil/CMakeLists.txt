###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

include_directories(${CMAKE_CURRENT_BINARY_DIR})

#TODO port following vpath code:
#ifeq (zos,$(OMR_HOST_OS))
	#OBJECTS += zbarrier$(OBJEXT)
	#ifeq (0,$(OMR_ENV_DATA64))
		#vpath zbarrier.s unix/zos/31
	#else
		#vpath zbarrier.s unix/zos/64
	#endif
#endif

include(OmrFindFiles)

set(OBJECTS "")
set(VPATH "")

add_tracegen(utilcore.tdf j9utilcore)

add_library(omrutil_obj OBJECT
	ut_j9utilcore.c
)

list(APPEND VPATH .)

if(OMR_ARCH_S390)
	if(OMR_HOST_OS STREQUAL zos)
		if(OMR_ENV_DATA64)
			list(APPEND OBJECTS j9memclrz10_64.s)
			list(APPEND OBJECTS omrget_userExtendedPrivateAreaMemoryType.s)

			list(APPEND VPATH unix/zos/64)
			target_include_directories(omrport PRIVATE unix/zos/64)
		else()
			list(APPEND OBJECTS omrceetbck.s)
			list(APPEND OBJECTS j9memclrz10_31.s)

			list(APPEND VPATH unix/zos/31)
			target_include_directories(omrport PRIVATE unix/zos/31)
		endif()
		list(APPEND OBJECTS zosversion.c)
		list(APPEND OBJECTS getstoragekey.c)
		list(APPEND OBJECTS getpsw.s)
		list(APPEND OBJECTS omrgetdsa.s)

		list(APPEND VPATH unix/zos)
		target_include_directories(omrport PRIVATE unix/zos)
	else()
		if(OMR_ENV_DATA64)
			list(APPEND OBJECTS j9memclrz10_64.s)

			list(APPEND VPATH unix/linux/s390/64)
			target_include_directories(omrport PRIVATE unix/linux/s390/64)
		else()
			list(APPEND OBJECTS j9memclrz10_31.s)

			list(APPEND VPATH unix/linux/s390/31)
			target_include_directories(omrport PRIVATE unix/linux/s390/31)
		endif()
		list(APPEND OBJECTS archinfo.c)

		list(APPEND VPATH unix/linux/s390)
		target_include_directories(omrport PRIVATE unix/linux/s390)
	endif()
endif()

if(OMR_ARCH_POWER)
	if(OMR_HOST_OS STREQUAL linux)
		if(OMR_ENV_DATA64)
			if(OMR_ENV_LITTLE_ENDIAN)
				list(APPEND OBJECTS gettoc.s)

				list(APPEND VPATH unix/linux/ppc/64le)
				target_include_directories(omrport PRIVATE unix/linux/ppc/64le)
			endif()
		endif()
	endif()
	if(NOT OMR_ENV_DATA64)
		list(APPEND OBJECTS cas8help.s)
		if(OMR_HOST_OS STREQUAL linux)
			list(APPEND VPATH unix/linux/ppc/32)
			target_include_directories(omrport PRIVATE unix/linux/ppc/32)
		else()
			list(APPEND VPATH unix/aix/32)
			target_include_directories(omrport PRIVATE unix/aix/32)
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

add_library(omrutil STATIC
	$<TARGET_OBJECTS:omrutil_obj>
)

if(OMR_WARNINGS_AS_ERRORS)
	target_compile_options(omrutil PRIVATE ${OMR_WARNING_AS_ERROR_FLAG})
endif()

if(OMR_ENHANCED_WARNINGS)
	target_compile_options(omrutil PRIVATE ${OMR_ENHANCED_WARNING_FLAG})
endif()

target_link_libraries(omrutil PUBLIC
	j9hashtable
	${OMR_UTIL_GLUE_TARGET}
)

set_target_properties(omrutil_obj omrutil PROPERTIES FOLDER util)
