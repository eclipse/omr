###############################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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

include(OmrFindFiles)

set(OBJECTS "")
set(VPATH "")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

add_tracegen(common/omrport.tdf)

# Logic below will set the target_includes, and supplement the
# target_sources
add_library(omrport_obj OBJECT
	ut_omrport.c
)


if(OMR_HOST_OS STREQUAL "aix")
	list(APPEND OBJECTS omrgetsp.s)
endif()


if(OMR_HOST_OS STREQUAL "zos")
	list(APPEND OBJECTS
		omrgenerate_ieat_dump.s
		omrget_large_pageable_pages_supported.s
		omrwto.s
		omrpgser_release.s
		omrgetuserid.c
		omrsysinfo_get_number_CPUs.s
		omrjobname.s
		omruserid.s
		omrzfs.c
		omrlpdat.mc
	)
	#TODO more objects here. See port_objects.mk:55
	if(OMR_ENV_DATA64)
		list(APPEND OBJECTS
			omrget_large_pages_supported.s
			omrget_large_2gb_pages_supported.s
			omrvmem_support_above_bar.s
			omrvmem_support_below_bar_64.s
			omript_ttoken64.s
		)
	else()
		list(APPEND OBJECTS omrvmem_support_below_bar_31.s)
	endif()
else()
	list(APPEND OBJECTS protect_helpers.c)
endif()

list(APPEND OBJECTS
	omrgetjobname.c
	omrgetjobid.c
	omrgetasid.c
)

if(OMR_ARCH_S390)
	list(APPEND OBJECTS omrrttime.s)
else()
	if(OMR_ARCH_POWER)
		if(OMR_ENV_DATA64)
			if(OMR_HOST_OS STREQUAL "linux")
				list(APPEND OBJECTS omrrttime.s)
			endif()
		endif()
	endif()
endif()

if(OMR_ARCH_S390)
	if(OMR_HOST_OS STREQUAL "linux")
		list(APPEND OBJECTS omrgetstfle.s)
	else()
		if(OMR_ENV_DATA64)
			list(APPEND OBJECTS omrgetstfle64.s)
		else()
			list(APPEND OBJECTS omrgetstfle31.s)
		endif()
	endif()
endif()


#TODO another if block @port_objects.mk:89

if(OMR_HOST_OS STREQUAL "aix")
	list(APPEND OBJECTS
		rt_divu64.s
		rt_time.s
	)
endif()

list(APPEND OBJECTS
	omrcpu.c
	omrerror.c
	omrerrorhelpers.c
	omrexit.c
	omrfile.c
	omrfiletext.c
	omrfilestream.c
	omrfilestreamtext.c
)

if(NOT OMR_HOST_OS STREQUAL "win")
	list(APPEND OBJECTS omriconvhelpers.c)
endif()

list(APPEND OBJECTS omrfile_blockingasync.c)

if(OMR_HOST_OS STREQUAL "win")
	list(APPEND OBJECTS omrfilehelpers.c)
endif()

if(OMR_ENV_DATA64)
	list(APPEND OBJECTS omrmem32helpers.c)
endif()

list(APPEND OBJECTS
	omrheap.c
	omrmem.c
	omrmemtag.c
	omrmemcategories.c
	omrport.c
	omrmmap.c
	j9nls.c
	j9nlshelpers.c
	omrosbacktrace.c
	omrosbacktrace_impl.c
	omrintrospect.c
	omrintrospect_common.c
	omrosdump.c
	omrportcontrol.c
	omrportptb.c

	omrsignal.c
)

if(NOT OMR_HOST_OS STREQUAL "win")
	list(APPEND OBJECTS omrsignal_context.c)
endif()

list(APPEND OBJECTS
	omrsl.c
	omrstr.c
	omrsysinfo.c
)

if(OMR_HOST_OS STREQUAL zos)
	list(APPEND OBJECTS omrsysinfo_helpers.c)
endif()

list(APPEND OBJECTS omrsyslog.c)

if(OMR_HOST_OS STREQUAL win)
	#TODO hack to compile mc files
	enable_language(RC)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/win32/omrsyslogmessages.rc
		COMMAND mc  ${CMAKE_CURRENT_SOURCE_DIR}/win32/omrsyslogmessages.mc
		DEPENDS win32/omrsyslogmessages.mc
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/win32
	)
	#TODO need to handle res files properly
	#list(APPEND OBJECTS omrsyslogmessages.res)
endif()

list(APPEND OBJECTS
	omrtime.c
	omrtlshelpers.c
	omrtty.c
	omrvmem.c
	omrmemtag_checks.c
)

if(OMR_HOST_OS STREQUAL aix)
	list(APPEND OBJECTS omrosdump_helpers.c)
elseif(OMR_HOST_OS STREQUAL linux)
	list(APPEND OBJECTS omrosdump_helpers.c)
elseif(OMR_HOST_OS STREQUAL osx)
	list(APPEND OBJECTS omrosdump_helpers.c)
endif()

if(OMR_HOST_OS STREQUAL zos)
	if(NOT OMR_ENV_DATA64)
		list(APPEND OBJECTS
			omrsignal_ceehdlr.c
			omrsignal_context_ceehdlr.c
		)
	endif()
endif()

if(OMR_HOST_ARCH STREQUAL ppc)
	if(OMR_HOST_OS STREQUAL linux)
		list(APPEND OBJECTS auxv.c)
	endif()
elseif(OMR_ARCH_S390)
	if(OMR_HOST_OS STREQUAL linux)
		list(APPEND OBJECTS auxv.c)
	endif()
endif()

if(OMR_OPT_CUDA)
	list(APPEND OBJECTS omrcuda.cpp)
endif()




#Setup paths and include directories

if(OMR_HOST_OS STREQUAL "zos")
	list(APPEND VPATH zos390)
	target_include_directories(omrport_obj PRIVATE zos390)
endif()

if(OMR_HOST_OS STREQUAL "win")
	if(OMR_ENV_DATA64)
		list(APPEND VPATH win64amd)
		target_include_directories(omrport_obj PRIVATE win64amd)
	endif()
endif()

if(OMR_HOST_OS STREQUAL "aix")
	#ifdef I5_VERSION
		#ifeq (1,$(OMR_ENV_DATA64))
			#vpath % $(PORT_SRCDIR)iseries64
			#MODULE_INCLUDES += $(PORT_SRCDIR)iseries64
		#endif
		#vpath % $(PORT_SRCDIR)iseries
		#MODULE_INCLUDES += $(PORT_SRCDIR)iseries
	#endif

	if(OMR_ENV_DATA64)
		list(APPEND VPATH aix64)
		target_include_directories(omrport_obj PRIVATE aix64)
	endif()
	list(APPEND VPATH aix)
	target_include_directories(omrport_obj PRIVATE aix)
endif()

if(OMR_HOST_OS STREQUAL "linux")
	if(OMR_ARCH_ARM)
		list(APPEND VPATH linuxarm)
		target_include_directories(omrport_obj PRIVATE linuxarm)
	endif()

	if(OMR_ARCH_AARCH64)
		list(APPEND VPATH linuxaarch64)
		target_include_directories(omrport_obj PRIVATE linuxaarch64)
	endif()

	if(OMR_ARCH_X86)
		if(OMR_ENV_DATA64)
			list(APPEND VPATH linuxamd64)
			target_include_directories(omrport_obj PRIVATE linuxamd64)
		else()
			list(APPEND VPATH linux386)
			target_include_directories(omrport_obj PRIVATE linux386)
		endif()

	endif()

	if(OMR_ARCH_POWER)
		if(OMR_ENV_DATA64)
			if(OMR_ENV_LITTLE_ENDIAN)
				list(APPEND VPATH linuxppc64le)
				target_include_directories(omrport_obj PRIVATE linuxppc64le)
			endif()
			list(APPEND VPATH linuxppc64)
			target_include_directories(omrport_obj PRIVATE linuxppc64)
		endif()
		list(APPEND VPATH linuxppc)
		target_include_directories(omrport_obj PRIVATE linuxppc)
	endif()

	if(OMR_ARCH_S390)
		if(OMR_ENV_DATA64)
			list(APPEND VPATH linuxs39064)
			target_include_directories(omrport_obj PRIVATE linuxs39064)
		endif()
		list(APPEND VPATH linuxs390)
		target_include_directories(omrport_obj PRIVATE linuxs390)
	endif()

	list(APPEND VPATH linux)
	target_include_directories(omrport_obj PRIVATE linux)
endif()


if(OMR_HOST_OS STREQUAL "osx")
	list(APPEND VPATH osx)
	target_include_directories(omrport_obj PRIVATE osx)
endif()


if(OMR_HOST_OS STREQUAL "win")
	list(APPEND VPATH win32_include win32)
	target_include_directories(omrport_obj PRIVATE win32_include win32)
else()
	list(APPEND VPATH unix)
	target_include_directories(omrport_obj PRIVATE unix unix_include)
endif()

list(APPEND VPATH common include)
target_include_directories(omrport_obj PRIVATE common include)

#TODO need to set some flags and etc see line 300

omr_find_files(resolvedPaths
	PATHS ${VPATH}
	FILES ${OBJECTS}
)


if(OMR_HOST_OS STREQUAL "win")
	list(APPEND resolvedPaths win32/omrsyslogmessages.rc)
endif()

target_sources(omrport_obj PRIVATE ${resolvedPaths})

add_library(omrport STATIC
	$<TARGET_OBJECTS:omrport_obj>
)

if(OMR_WARNINGS_AS_ERRORS)
	target_compile_options(omrport_obj PRIVATE ${OMR_WARNING_AS_ERROR_FLAG})
endif()

if(OMR_ENHANCED_WARNINGS)
	target_compile_options(omrport_obj PRIVATE ${OMR_ENHANCED_WARNING_FLAG})
endif()

target_link_libraries(omrport
	PRIVATE
	j9avl
	j9hashtable
	j9pool
	${OMR_THREAD_LIB}
)

target_include_directories(omrport_obj PRIVATE
	../nls
)

# This flag indicates that we are compiling the port library
target_compile_definitions(omrport_obj PRIVATE -DOMRPORT_LIBRARY_DEFINE)



#TODO hack to get to compile. Need platform checks
if(NOT OMR_HOST_OS STREQUAL "win")
	if(NOT OMR_HOST_OS STREQUAL "zos")
		target_link_libraries(omrport PRIVATE dl)
	endif()
endif()

if(OMR_HOST_OS STREQUAL "osx")
	target_link_libraries(omrport PRIVATE iconv)
endif()

if(OMR_HOST_OS STREQUAL "win")
	target_link_libraries(omrport PRIVATE psapi pdh)
endif()
