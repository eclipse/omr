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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
#############################################################################

list(APPEND OMR_PLATFORM_DEFINITIONS
	-DJ9ZOS390
	-DLONGLONG
	-D_ALL_SOURCE
	-D_XOPEN_SOURCE_EXTENDED
	-DIBM_ATOE
	-D_POSIX_SOURCE
)

list(APPEND OMR_PLATFORM_INCLUDE_DIRECTORIES
	${CMAKE_SOURCE_DIR}/util/a2e/headers
	/usr/lpp/cbclib/include
	/usr/include
)

list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
	"\"-Wc,xplink\""               # link with xplink calling convention
	"\"-Wc,convlit(ISO8859-1)\""   # convert all string literals to a codepage
	"\"-Wc,rostring\""             # place string literals in read only storage
	"\"-Wc,FLOAT(IEEE,FOLD,AFP)\"" # Use IEEE (instead of IBM Hex Format) style floats
	"\"-Wc,enum(4)\""              # Specifies how many bytes of storage enums occupy
	"\"-Wa,goff\""                 # Assemble into GOFF object files
	"\"-Wc,NOANSIALIAS\""          # Do not generate ALIAS binger control statements
	"\"-Wc,TARGET(zOSV1R13)\""     # Generate code for the target operating system
)

list(APPEND OMR_PLATFORM_C_COMPILE_OPTIONS
	"\"-Wc,ARCH(7)\""
	"\"-Wc,langlvl(extc99)\""
	"\"-qnosearch\""
)

list(APPEND OMR_PLATFORM_CXX_COMPILE_OPTIONS
	"\"-Wc,ARCH(7)\""
	"\"-Wc,langlvl(extended)\""
	"\"-qnosearch\""
)

list(APPEND OMR_PLATFORM_SHARED_LINKER_OPTIONS
	"\"-Wl,xplink\""
	"\"-Wl,dll\""
)

if(OMR_ENV_DATA64)
	list(APPEND OMR_PLATFORM_DEFINITIONS
		-DJ9ZOS39064
	)
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
		\"-Wc,lp64\"
		\"-Wa,SYSPARM(BIT64)\"
	)
else()
	list(APPEND OMR_PLATFORM_DEFINITIONS
		-D_LARGE_FILES
	)
endif()

macro(omr_os_global_setup)

	# TODO: Move this out and after platform config.
	enable_language(ASM-ZOS)

	omr_append_flags(CMAKE_ASM-ZOS_FLAGS ${OMR_PLATFORM_COMPILE_OPTIONS})

	target_compile_options(omr_shared INTERFACE "-Wc,DLL,EXPORTALL")

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

endmacro(omr_os_global_setup)
