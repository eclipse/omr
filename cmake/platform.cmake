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


###
### Platform flags
### TODO: arch flags. Defaulting to x86-64

message(STATUS "CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM}")
message(STATUS "CMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_CROSSCOMPILING=${CMAKE_CROSSCOMPILING}")

# Remove a specified option from a variable
macro(omr_remove_option var opt)
    string( REGEX REPLACE
		"(^| )${opt}($| )"
		""
		${var}
		"${${var}}"
	)
endmacro(omr_remove_option)

# TODO: Support all system types known in OMR
# TODO: Is there a better way to do system flags?

if((CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64") OR (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64"))
	set(OMR_ARCH_X86 ON)
	set(OMR_ENV_DATA64 ON)
	set(OMR_TARGET_DATASIZE 64)
elseif((CMAKE_SYSTEM_PROCESSOR STREQUAL "x86") OR
	   (CMAKE_SYSTEM_PROCESSOR STREQUAL "i386") OR
	   (CMAKE_SYSTEM_PROCESSOR STREQUAL "i686"))
	set(OMR_ARCH_X86 ON)
	set(OMR_ENV_DATA32 ON)
	set(OMR_TARGET_DATASIZE 32)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
	set(OMR_ARCH_ARM ON)
	set(OMR_ENV_DATA32 ON)
	set(OMR_TARGET_DATASIZE 32)
elseif(CMAKE_SYSTEM_NAME MATCHES "OS390")
	set(OMR_ARCH_S390 ON)
	set(OMR_ENV_DATA32 ON)
	set(OMR_TARGET_DATASIZE 32)
	set(OMR_HOST_OS "zos")
else()
	message(FATAL_ERROR "Unknown processor: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

#TODO: detect other Operating systems (aix, and zos)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(OMR_HOST_OS "linux")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(OMR_HOST_OS "osx")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(OMR_HOST_OS "win")
elseif(OMR_HOST_OS STREQUAL "zos")
	# we already set up OS stuff above
else()
	message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()


if(OMR_HOST_OS STREQUAL "linux")
	add_definitions(
		-DLINUX
		-D_REENTRANT
		-D_FILE_OFFSET_BITS=64
	)
endif()


if(OMR_ARCH_X86)
	set(OMR_ENV_LITTLE_ENDIAN ON CACHE BOOL "TODO: Document")
	if(OMR_ENV_DATA64)
		add_definitions(-DJ9HAMMER)
	else()
		add_definitions(-DJ9X86)
	endif()
endif()

if(OMR_ARCH_ARM)
	set(OMR_ENV_LITTLE_ENDIAN ON CACHE BOOL "todo: Document")
	add_definitions(
		-DJ9ARM
		-DARMGNU
		-DFIXUP_UNALIGNED
	)
endif()

if(OMR_HOST_OS STREQUAL "osx")
	add_definitions(
		-DOSX
		-D_REENTRANT
		-D_FILE_OFFSET_BITS=64
	)
endif()

# interface library for exporting symbols from dynamic library
# currently does nothing except on zos
add_library(omr_shared INTERFACE)

if(OMR_HOST_OS STREQUAL "win")
	#TODO: should probably check for MSVC
	set(OMR_WINVER "0x501")

	add_definitions(
		-DWIN32
		-D_CRT_SECURE_NO_WARNINGS
		-DCRTAPI1=_cdecl
		-DCRTAPI2=_cdecl
		-D_WIN_95
		-D_WIN32_WINDOWS=0x0500
		-D_WIN32_DCOM
		-D_MT
		-D_WINSOCKAPI_
		-D_WIN32_WINVER=${OMR_WINVER}
		-D_WIN32_WINNT=${OMR_WINVER}
		-D_DLL
	)
	if(OMR_ENV_DATA64)
		add_definitions(
			-DWIN64
			-D_AMD64_=1
		)
		set(TARGET_MACHINE AMD64)
	else()
		add_definitions(
			-D_X86_
		)
		set(TARGET_MACHINE i386)
	endif()
	set(opt_flags "/GS-")
	set(common_flags "-MD -Zm400")

	set(linker_common "-subsystem:console -machine:${TARGET_MACHINE}")
	if(NOT OMR_ENV_DATA64)
		set(linker_common "${linker_common} /SAFESEH")
	endif()

	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${linker_common} /LARGEADDRESSAWARE wsetargv.obj")
	if(OMR_ENV_DATA64)
		#TODO: makefile has this but it seems to break linker
		#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRTD")
	endif()


	# Make sure we are building without incremental linking
	omr_remove_option(CMAKE_EXE_LINKER_FLAGS "/INCREMENTAL")
	omr_remove_option(CMAKE_SHARED_LINKER_FLAGS "/INCREMENTAL")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL:NO /NOLOGO")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL:NO /NOLOGO")
	foreach(build_type IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${build_type}" build_type)
		omr_remove_option("CMAKE_EXE_LINKER_FLAGS_${build_type}" "/INCREMENTAL")
		omr_remove_option("CMAKE_SHARED_LINKER_FLAGS_${build_type}" "/INCREMENTAL")
	endforeach()

	if(OMR_ENV_DATA64)
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -entry:_DllMainCRTStartup")
	else()
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -entry:_DllMainCRTStartup@12")
	endif()

	#strip out exception handling flags (added by default by cmake)
	omr_remove_option(CMAKE_CXX_FLAGS "/EHsc")
	omr_remove_option(CMAKE_CXX_FLAGS "/GR")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${common_flags}")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${common_flags}")

	message(STATUS "CFLAGS = ${CMAKE_C_FLAGS}")
	message(STATUS "CXXFLAGS = ${CMAKE_CXX_FLAGS}")

	#Hack up output dir to fix dll dependency issues on windows
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

if(OMR_HOST_OS STREQUAL "zos")

	enable_language(ASM-ZOS)

	include_directories(util/a2e/headers)

	add_definitions(
		-DJ9ZOS390
		-DLONGLONG
		-DJ9VM_TIERED_CODE_CACHE
		-D_ALL_SOURCE
		-D_XOPEN_SOURCE_EXTENDED
		-DIBM_ATOE
		-D_POSIX_SOURCE
	)

	#NOTE: character escaping on compiler args seems broken

	# Global Flags
	# xplink   Link with the xplink calling convention
	# convlit  Convert all string literals to a codepage
	# rostring Place string literals in read only storage
	# FLOAT    Use IEEE (instead of IBM Hex Format) style floats
	# enum     Specifies how many bytes of storage enums occupy
	# a,goff   Assemble into GOFF object files
	# NOANSIALIAS Do not generate ALIAS binder control statements
	# TARGET   Generate code for the target operating system
	# list     Generate assembly listing
	# offset   In assembly listing, show addresses as offsets of function entry points
	#set(OMR_GLOBAL_FLAGS "-Wc,xplink,convlit\\\(ISO8859-1\\\),rostring,FLOAT\\\(IEEE,FOLD,AFP\\\),enum\\\(4\\\) -Wa,goff -Wc,NOANSIALIAS -Wc,TARGET\\\(zOSV1R13\\\) -W \"c,list,offset\"")
	set(OMR_GLOBAL_FLAGS "-Wc,xplink,convlit\\\(ISO8859-1\\\),rostring,FLOAT\\\(IEEE,FOLD,AFP\\\),enum\\\(4\\\) -Wa,goff -Wc,NOANSIALIAS -Wc,TARGET\\\(zOSV1R13\\\)")

	if(OMR_ENV_DATA64)
		add_definitions(-DJ9ZOS39064)
		set(OMR_GLOBAL_FLAGS "${OMR_GLOBAL_FLAGS} -Wc,lp64 -Wa,SYSPARM\\\(BIT64\\\)")
	else()
		add_definitions(-D_LARGE_FILES)
	endif()


	set(CMAKE_ASM-ZOS_FLAGS "${OMR_GLOBAL_FLAGS}")
	set(OMR_GLOBAL_FLAGS "${OMR_GLOBAL_FLAGS} -qnosearch -I${CMAKE_SOURCE_DIR}/util/a2e/headers -I/usr/include")
	# Specify the minimum arch for 64-bit programs
	#TODO this is a gross hack

	set(CMAKE_C_FLAGS "-Wc,ARCH\\\(7\\\) -Wc,\"langlvl(extc99)\" ${OMR_GLOBAL_FLAGS}")
	set(CMAKE_CXX_FLAGS "-Wc,ARCH\\\(7\\\) -Wc,\"langlvl(extended)\" ${OMR_GLOBAL_FLAGS} -I/usr/lpp/cbclib/include")



	target_compile_options(omr_shared INTERFACE "-Wc,DLL,EXPORTALL")

	set(CMAKE_SHARED_LINKER_FLAGS "-Wl,xplink,dll")

	#TODO below is a chunk of the original makefile which still needs to be ported
	# # This is the first option applied to the C++ linking command.
	# # It is not applied to the C linking command.
	# OMR_MK_CXXLINKFLAGS=-Wc,"langlvl(extended)" -+

	# ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))
		# GLOBAL_LDFLAGS+=-Wl,xplink,dll
	# else
		# # Assume we're linking an executable
		# GLOBAL_LDFLAGS+=-Wl,xplink
	# endif
	# ifeq (1,$(OMR_ENV_DATA64))
		# OMR_MK_CXXLINKFLAGS+=-Wc,lp64
		# GLOBAL_LDFLAGS+=-Wl,lp64
	# endif

	# # always link a2e last, unless we are creating the a2e library
	# ifneq (j9a2e,$(MODULE_NAME))
		# GLOBAL_SHARED_LIBS+=j9a2e
	# endif

	#dump DLLs and exes in same dir like on windows
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
	#apparently above doesnt work like it does on windows. Attempt to work arround
	set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
	set(LIBRARY_OUTPUT_PATH  "${CMAKE_BINARY_DIR}")



	message(STATUS "DEBUG: RUNTIME_OUTPUT_DIR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
	message(STATUS "DEBUG: CFLAGS=${CMAKE_C_FLAGS}")
	message(STATUS "DEBUG: EXE LDFLAGS=${CMAKE_EXE_LINKER_FLAGS}")
	message(STATUS "DEBUG: so LDFLAGS=${CMAKE_SHARED_LINKER_FLAGS}")

endif()

###
### Flags we aren't using
###

# TODO: SPEC

# TODO: OMR_HOST_ARCH
# TODO: OMR_TARGET_DATASIZE
# TODO: OMR_TOOLCHAIN
# TODO: OMR_CROSS_CONFIGURE
# TODO: OMR_RTTI

