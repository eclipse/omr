###############################################################################
#
# (c) Copyright IBM Corp. 2017
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################



set(OBJECTS "")
set(VPATH "")

# Logic below will set the target_includes, and supplement the 
# target_sources
add_library(omrport STATIC 
	ut_omrport.c
)
omr_set_target_flags(omrport)


if(OMR_HOST_OS STREQUAL "aix")
	list(APPEND OBJECTS omrgetsp.c)
endif()


if(OMR_HOST_OS STREQUAL "zos")
	list(APPEND OBJECTS
		j9generate_ieat_dump.s
		omrget_large_pageable_pages_supported.s
		j9wto.s
		j9pgser_release.s
		omrgetuserid.c
		j9sysinfo_get_number_CPUs.s
		j9jobname.s
		j9userid.s
		j9csrsi.c
		j9csrsi_wrp.s
		j9zfs.c
		j9lpdat.mc
	)
	#TODO more objects here. See port_objects.mk:55
	if(OMR_ENV_DATA64)
		list(APPEND OBJECTS
			omrget_userExtendedPrivateAreaMemoryType.s
			omrget_large_pages_supported.s
			omrget_large_2gb_pages_supported.s
			omrvmem_support_above_bar.s
			omrvmem_support_below_bar_64.s
			j9ipt_ttoken64.s
		)
	else()
		list(APPEND OBJECTS omrvmem_support_below_bar_31.c)
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
	if(OMR_ARCH_PPC)
		if(OMR_ENV_DATA64)
			if(OMR_HOST_OS STREQUAL "linux")
				#TODO what source extension is this?
				list(APPEND OBJECTS omrrttime)
			endif()
		endif()
	endif()
endif()

if(OMR_ARCH_S390)
	if(OMR_HOST_OS STREQUAL "linux")
		list(APPEND OBJECTS omrgetstfle) #TODO: extension?
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
		rt_divu64.c
		rt_time.c
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
endif()

if(OMR_OPT_CUDA)
	list(APPEND OBJECTS omrcuda.cpp)
endif()




#Setup paths and include directories

if(OMR_HOST_OS STREQUAL "zos")
	list(APPEND VPATH zos390)
	target_include_directories(omrport PRIVATE zos390)
endif()

if(OMR_HOST_OS STREQUAL "win")
	if(OMR_ENV_DATA64)
		list(APPEND VPATH win64amd)
		target_include_directories(omrport PRIVATE win64amd)
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
	#vpath % $(PORT_SRCDIR)aix64
	#MODULE_INCLUDES += $(PORT_SRCDIR)aix64
	endif()
	#vpath % $(PORT_SRCDIR)aix
	#MODULE_INCLUDES += $(PORT_SRCDIR)aix
endif()


#endif
#TODO other platforms port_objects:209

if(OMR_HOST_OS STREQUAL "linux")
	#TODO need to port following vpath code
	#ifeq (ppc,$(OMR_HOST_ARCH))
		#ifeq (1,$(OMR_ENV_DATA64))
			#ifeq (1,$(OMR_ENV_LITTLE_ENDIAN))
				#vpath % $(PORT_SRCDIR)linuxppc64le
				#MODULE_INCLUDES += $(PORT_SRCDIR)linuxppc64le
			#endif
			#vpath % $(PORT_SRCDIR)linuxppc64
			#MODULE_INCLUDES += $(PORT_SRCDIR)linuxppc64
		#endif
		#vpath % $(PORT_SRCDIR)linuxppc
		#MODULE_INCLUDES += $(PORT_SRCDIR)linuxppc
	#endif

	#ifeq (s390,$(OMR_HOST_ARCH))
		#ifeq (1,$(OMR_ENV_DATA64))
			#vpath % $(PORT_SRCDIR)linuxs39064
			#MODULE_INCLUDES += $(PORT_SRCDIR)linuxs39064
		#endif
		#vpath % $(PORT_SRCDIR)linuxs390
		#MODULE_INCLUDES += $(PORT_SRCDIR)linuxs390
	#endif

	if(OMR_ARCH_ARM)
		list(APPEND VPATH linuxarm)
		target_include_directories(omrport PRIVATE linuxarm)
	endif()

	if(OMR_ARCH_X86)
		if(OMR_ENV_DATA64)
			list(APPEND VPATH linuxamd64)
			target_include_directories(omrport PRIVATE linuxamd64)
		else()
			list(APPEND VPATH linux386)
			target_include_directories(omrport PRIVATE linux386)
		endif()

	endif()

	list(APPEND VPATH linux)
	target_include_directories(omrport PRIVATE linux)
endif()


if(OMR_HOST_OS STREQUAL "osx")
	list(APPEND VPATH osx_include osx)
	target_include_directories(omrport PRIVATE osx_include osx)
endif()


if(OMR_HOST_OS STREQUAL "win")
	list(APPEND VPATH win32_include win32)
	target_include_directories(omrport PRIVATE win32_include win32)
else()
	list(APPEND VPATH unix)
	target_include_directories(omrport PRIVATE unix unix_include)
endif()

list(APPEND VPATH common include)
target_include_directories(omrport PRIVATE common include)

#TODO need to set some flags and etc see line 300


set(resolvedPaths "")
VPathResolve(OBJECTS VPATH resolvedPaths)


if(OMR_HOST_OS STREQUAL "win")
    list(APPEND resolvedPaths win32/omrsyslogmessages.rc)
endif()

add_tracegen(common/omrport.tdf)

target_sources(omrport PRIVATE ${resolvedPaths})

target_link_libraries(omrport
    PRIVATE
	j9thrstatic
	j9hashtable
)

#TODO: at the moment this assumes all C source files which is a lie
target_include_directories(omrport PRIVATE 
	../nls
)

# This flag indicates that we are compiling the port library
target_compile_definitions(omrport PRIVATE -DOMRPORT_LIBRARY_DEFINE)



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
