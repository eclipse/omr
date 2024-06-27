###############################################################################
# Copyright IBM Corp. and others 2017
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
#############################################################################

include(CheckSymbolExists)

# Determine semaphore implementation to use on platform.
macro(omr_find_semaphore_implementation)
	set(OMR_SEMAPHORE_IMPLEMENTATION "-NOTFOUND" CACHE STRING "What semaphore implementation to use")
	set_property(CACHE OMR_SEMAPHORE_IMPLEMENTATION PROPERTY STRINGS osx posix windows zos)
	mark_as_advanced(OMR_SEMAPHORE_IMPLEMENTATION)
	if(NOT OMR_SEMAPHORE_IMPLEMENTATION)
		if(OMR_OS_OSX)
			set(OMR_SEMAPHORE_IMPLEMENTATION osx)
		elseif(OMR_OS_WINDOWS)
			set(OMR_SEMAPHORE_IMPLEMENTATION windows)
		elseif(OMR_OS_ZOS)
			set(OMR_SEMAPHORE_IMPLEMENTATION zos)
		else()
			# For now we assume that if sem_init exists, the rest of the POSIX semaphore functions are there.
			# Note: cmake requires that test source be able to compile and link, which requires us to tell it
			# to link against pthread.
			set(_OLD_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}")
			set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} pthread)
			check_symbol_exists(sem_init "semaphore.h" HAS_SEM_INIT)
			set(CMAKE_REQUIRED_LIBRARIES "${_OLD_REQUIRED_LIBRARIES}")
			if(HAS_SEM_INIT)
				set(OMR_SEMAPHORE_IMPLEMENTATION posix)
			endif()
		endif()
		# Force value of OMR_SEMAPHORE_IMPLEMENTATION.
		set_property(CACHE OMR_SEMAPHORE_IMPLEMENTATION PROPERTY VALUE "${OMR_SEMAPHORE_IMPLEMENTATION}")
	endif()

	unset(OMR_USE_OSX_SEMAPHORES)
	unset(OMR_USE_POSIX_SEMAPHORES)
	unset(OMR_USE_WINDOWS_SEMAPHORES)
	unset(OMR_USE_ZOS_SEMAPHORES)

	if(OMR_SEMAPHORE_IMPLEMENTATION STREQUAL osx)
		set(OMR_USE_OSX_SEMAPHORES TRUE)
	elseif(OMR_SEMAPHORE_IMPLEMENTATION STREQUAL posix)
		set(OMR_USE_POSIX_SEMAPHORES TRUE)
	elseif(OMR_SEMAPHORE_IMPLEMENTATION STREQUAL windows)
		set(OMR_USE_WINDOWS_SEMAPHORES TRUE)
	elseif(OMR_SEMAPHORE_IMPLEMENTATION STREQUAL zos)
		set(OMR_USE_ZOS_SEMAPHORES TRUE)
	else()
		message(SEND_ERROR "'${OMR_SEMAPHORE_IMPLEMENTATION}' is not a valid value for OMR_SEMAPHORE_IMPLEMENTATION")
	endif()
endmacro()

function(omr_check_dladdr)
	list(APPEND CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE")
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${CMAKE_DL_LIBS})
	check_symbol_exists(dladdr "dlfcn.h" OMR_HAVE_DLADDR)
endfunction()

# Translate from CMake's view of the system to the OMR view of the system.
# Exports a number of variables indicating platform, os, endianness, etc.
# - OMR_ARCH_{AARCH64,X86,ARM,S390,POWER}
# - OMR_ENV_DATA{32,64}
# - OMR_ENV_TARGET_DATASIZE (either 32 or 64)
# - OMR_ENV_LITTLE_ENDIAN
# - OMR_HOST_OS
macro(omr_detect_system_information)
	set(OMR_TEMP_DATA_SIZE "unknown")

	if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
		set(OMR_HOST_ARCH "x86")
		set(OMR_ARCH_X86 ON)
		set(OMR_ENV_LITTLE_ENDIAN ON)
		set(OMR_TEMP_DATA_SIZE "64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86|i[3-6]86")
		set(OMR_HOST_ARCH "x86")
		set(OMR_ARCH_X86 ON)
		set(OMR_ENV_LITTLE_ENDIAN ON)
		set(OMR_TEMP_DATA_SIZE "32")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
		set(OMR_HOST_ARCH "aarch64")
		set(OMR_ARCH_AARCH64 ON)
		set(OMR_ENV_LITTLE_ENDIAN ON)
		set(OMR_TEMP_DATA_SIZE "64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
		set(OMR_HOST_ARCH "arm")
		set(OMR_ARCH_ARM ON)
		set(OMR_ENV_LITTLE_ENDIAN ON)
		set(OMR_TEMP_DATA_SIZE "32")
	elseif(CMAKE_SYSTEM_NAME MATCHES "OS390")
		set(OMR_HOST_ARCH "s390")
		set(OMR_ARCH_S390 ON)
		set(OMR_TEMP_DATA_SIZE "64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
		set(OMR_HOST_ARCH "ppc")
		set(OMR_ARCH_POWER ON)
		set(OMR_ENV_LITTLE_ENDIAN ON)
		set(OMR_TEMP_DATA_SIZE "64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "s390x")
		set(OMR_HOST_ARCH "s390")
		set(OMR_ARCH_S390 ON)
		set(OMR_TEMP_DATA_SIZE "64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "powerpc")
		set(OMR_HOST_ARCH "ppc")
		set(OMR_ARCH_POWER ON)
		set(OMR_TEMP_DATA_SIZE "64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc")
		set(OMR_HOST_ARCH "ppc")
		set(OMR_ARCH_POWER ON)
		set(OMR_TEMP_DATA_SIZE "32")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
		set(OMR_HOST_ARCH "riscv")
		set(OMR_ARCH_RISCV ON)
		set(OMR_ENV_LITTLE_ENDIAN ON)
		set(OMR_TEMP_DATA_SIZE "64")
	else()
		message(FATAL_ERROR "Unknown processor: ${CMAKE_SYSTEM_PROCESSOR}")
	endif()

	if(NOT (OMR_ENV_DATA64 OR OMR_ENV_DATA32))
		# env data size was not set in the cache or via the command line
		if(OMR_TEMP_DATA_SIZE STREQUAL 64)
			set(OMR_ENV_DATA64 ON)
			set(OMR_ENV_TARGET_DATASIZE 64)
		elseif(OMR_TEMP_DATA_SIZE STREQUAL 32)
			set(OMR_ENV_DATA32 ON)
			set(OMR_ENV_TARGET_DATASIZE 32)
		else()
			message(FATAL_ERROR "Error setting default env data size")
		endif()
	else()
		# env data size was set in the cache or on the command line
		if(OMR_ENV_DATA64)
			# TODO assert OMR_ENV_DATA32 is not also set
			set(OMR_ENV_TARGET_DATASIZE 64)
		elseif(OMR_ENV_DATA32)
			# TODO assert OMR_ENV_DATA64 is not also set
			set(OMR_ENV_TARGET_DATASIZE 32)
		else()
			message(FATAL_ERROR "Error configuring the env data size")
		endif()
	endif()

	if(CMAKE_SYSTEM_NAME MATCHES "Linux")
		set(OMR_HOST_OS "linux")
		set(OMR_OS_LINUX TRUE)
	elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
		set(OMR_HOST_OS "osx")
		set(OMR_OS_OSX TRUE)
	elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
		set(OMR_HOST_OS "win")
		set(OMR_OS_WINDOWS TRUE)
	elseif(CMAKE_SYSTEM_NAME MATCHES "AIX")
		set(OMR_HOST_OS "aix")
		set(OMR_OS_AIX TRUE)
	elseif(CMAKE_SYSTEM_NAME MATCHES "OS390")
		set(OMR_HOST_OS "zos")
		set(OMR_OS_ZOS TRUE)
	elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
		set(OMR_HOST_OS "bsd")
		set(OMR_OS_BSD TRUE)
	else()
		message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
	endif()

	# NOTE: This is based off of the id of the C compiler. This means that
	# if you mix and match c/c++ compilers it all falls apart,
	# but that seems like a case we probably don't want to support anyway.
	if(NOT CMAKE_C_COMPILER_ID)
		message(WARNING "OMR: C Compiler ID is not set. Did you call omr_detect_system_information before C was enabled?")
	else()
		if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
			set(_OMR_TOOLCONFIG "gnu")
		elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
			set(_OMR_TOOLCONFIG "msvc")
		elseif(CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang$")
			if("${CMAKE_C_SIMULATE_ID}" STREQUAL "MSVC" OR "${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
				# clang on Windows mimics MSVC
				set(_OMR_TOOLCONFIG "msvc")
			else()
				# TODO we don't actually have a clang config
				# just use GNU config
				set(_OMR_TOOLCONFIG "gnu")
			endif()
		elseif(CMAKE_C_COMPILER_ID MATCHES "IBMClang$")
			set(_OMR_TOOLCONFIG "openxl")
			set(CMAKE_C_COMPILER_IS_OPENXL ON CACHE BOOL "ibm-clang is the C compiler")
		elseif(CMAKE_C_COMPILER_ID MATCHES "^XL(Clang)?$" OR CMAKE_C_COMPILER_ID STREQUAL "zOS")
			# In CMake 3.14 and prior, XLClang uses CMAKE_C_COMPILER_ID "XL"
			# In CMake 3.15 and beyond, XLClang uses CMAKE_C_COMPILER_ID "XLClang"
			set(_OMR_TOOLCONFIG "xlc")
			message(STATUS "CMAKE_C_COMPILER is ${CMAKE_C_COMPILER}")
			if(CMAKE_C_COMPILER MATCHES ".*xlclang$")
				# Checking the CMAKE_C_COMPILER command is necessary to determine if XLClang is
				# the compiler, since XLClang might have CMAKE_C_COMPILER_ID "XL" or "XLClang"
				# depending on the CMake version. Without this check, it's ambiguous whether the
				# compiler is XLC or XLClang.
				set(CMAKE_C_COMPILER_IS_XLCLANG TRUE CACHE BOOL "XLClang is the C compiler")
			elseif(CMAKE_C_COMPILER MATCHES ".*ibm-clang.*")
				set(CMAKE_C_COMPILER_IS_OPENXL ON CACHE BOOL "ibm-clang is the C compiler")
				set(_OMR_TOOLCONFIG "openxl")
			else()
				message(STATUS "NO XLClang match. Using xlc toolchain.")
			endif()
		else()
			message(FATAL_ERROR "OMR: Unknown compiler ID: '${CMAKE_C_COMPILER_ID}'")
		endif()
		set(OMR_TOOLCONFIG ${_OMR_TOOLCONFIG} CACHE STRING "Name of toolchain configuration options to use")
	endif()

	message(STATUS "OMR: The CPU architecture is ${OMR_HOST_ARCH}")
	message(STATUS "OMR: The OS is ${OMR_HOST_OS}")
	message(STATUS "OMR: The tool configuration is ${OMR_TOOLCONFIG}")
	message(STATUS "OMR: The target data size is ${OMR_ENV_TARGET_DATASIZE}")

	omr_find_semaphore_implementation()
	omr_check_dladdr()

	# Check if we are a multi config generator.
	# CMake 3.10 adds GENERATOR_IS_MULTI_CONFIG property
	# otherwise Visual Studio and XCode are the only multi config generators.
	if(CMAKE_VERSION VERSION_LESS 3.10)
		get_property(OMR_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
	else()
		if(CMAKE_GENERATOR MATCHES "^Visual Studio|^Xcode")
			set(OMR_MULTI_CONFIG TRUE)
		else()
			set(OMR_MULTI_CONFIG FALSE)
		endif()
	endif()
endmacro(omr_detect_system_information)
