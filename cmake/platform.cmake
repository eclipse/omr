##############################################################################
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

include(cmake/DetectSystemInformation.cmake)
omr_detect_system_information()

# Pickup OS info 
include(cmake/platform/os/${OMR_HOST_OS}.cmake OPTIONAL)

# Pickup Arch Info
include(cmake/platform/arch/${OMR_HOST_ARCH}.cmake OPTIONAL) 

# Pickup toolconfig based on platform. 
include(cmake/platform/toolcfg/${OMR_TOOLCONFIG}.cmake OPTIONAL)

# Verify toolconfig!
include(cmake/platform/toolcfg/verify.cmake)

include(cmake/AddPrefix.cmake)

# Remove a specified option from a variable
macro(omr_remove_option var opt)
	string( REGEX REPLACE
		"(^| )${opt}($| )"
		""
		${var}
		"${${var}}"
	)
endmacro(omr_remove_option)


add_prefix(OMR_OS_DEFINITIONS_PREFIXED   ${OMR_C_DEFINITION_PREFIX} ${OMR_OS_DEFINITIONS})
add_prefix(OMR_ARCH_DEFINITIONS_PREFIXED ${OMR_C_DEFINITION_PREFIX} ${OMR_ARCH_DEFINITIONS})


add_definitions(
   ${OMR_OS_COMPILE_OPTIONS}
   ${OMR_OS_DEFINITIONS_PREFIXED}
   ${OMR_ARCH_DEFINITIONS_PREFIXED}
   )

if(OMR_HOST_OS STREQUAL "linux")
	if(OMR_WARNINGS_AS_ERRORS)
           set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${OMR_WARNING_AS_ERROR_FLAG}")
           set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OMR_WARNING_AS_ERROR_FLAG}")
	endif()
endif()


# interface library for exporting symbols from dynamic library
# currently does nothing except on zos
add_library(omr_shared INTERFACE)

if(OMR_HOST_OS STREQUAL "win")

	set(opt_flags "/GS-")
	# we want to disable C4091, and C4577
	# C4577 is a bogus warning about specifying noexcept when exceptions are disabled
	# C4091 is caused by broken windows sdk (https://connect.microsoft.com/VisualStudio/feedback/details/1302025/warning-c4091-in-sdk-7-1a-shlobj-h-1051-dbghelp-h-1054-3056)
	set(common_flags "-MD -Zm400 /wd4577 /wd4091")

	if(OMR_WARNINGS_AS_ERRORS)
           set(common_flags "${common_flags} ${OMR_WARNING_AS_ERROR_FLAG}")
		# TODO we also want to be setting warning as error on linker flags
	endif()

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

