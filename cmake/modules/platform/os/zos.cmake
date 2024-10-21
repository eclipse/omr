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

list(APPEND OMR_PLATFORM_DEFINITIONS
	-D_ALL_SOURCE
	-D_XOPEN_SOURCE=600
	-D_OPEN_THREADS=3
	-D__STDC_LIMIT_MACROS
	-DLONGLONG
	-DJ9ZOS390
	-DSUPPORTS_THREAD_LOCAL
	-DZOS
)

# CMake ignores any include directories which appear in IMPLICIT_INCLUDE_DIRECTORIES.
# This causes an issue with a2e since we need to re-specify them after clearing default search path.
list(REMOVE_ITEM CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES /usr/include)
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES /usr/include)

# Make sure that cmake can find libelf/libdwarf headers.
list(APPEND CMAKE_INCLUDE_PATH "/usr/lpp/cbclib/include")

# Create helper targets for specifying ascii/ebcdic options
add_library(omr_ascii INTERFACE)
target_compile_definitions(omr_ascii INTERFACE -DIBM_ATOE)
if(CMAKE_C_COMPILER_IS_OPENXL)
	target_compile_options(omr_ascii INTERFACE -fexec-charset=ISO8859-1 -isystem ${CMAKE_CURRENT_LIST_DIR}/../../../../util/a2e/headers)
else()
	target_compile_options(omr_ascii INTERFACE "-Wc,convlit(ISO8859-1),nose,se(${CMAKE_CURRENT_LIST_DIR}/../../../../util/a2e/headers)")
endif()
target_link_libraries(omr_ascii INTERFACE j9a2e)

add_library(omr_ebcdic INTERFACE)
target_compile_definitions(omr_ebcdic INTERFACE -DOMR_EBCDIC)

install(TARGETS omr_ascii omr_ebcdic
	EXPORT OmrTargets
)

macro(omr_os_global_setup)
	# TODO below is a chunk of the original makefile which still needs to be ported
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

	# dump DLLs and exes in same dir like on Windows
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
	# Apparently above doesn't work like it does on Windows. Attempt to work around.
	set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
	set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

	message(STATUS "DEBUG: RUNTIME_OUTPUT_DIR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
	message(STATUS "DEBUG: CFLAGS=${CMAKE_C_FLAGS}")
	message(STATUS "DEBUG: EXE LDFLAGS=${CMAKE_EXE_LINKER_FLAGS}")
	message(STATUS "DEBUG: so LDFLAGS=${CMAKE_SHARED_LINKER_FLAGS}")
endmacro(omr_os_global_setup)
