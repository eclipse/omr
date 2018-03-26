###############################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#############################################################################

set(OMR_WARNING_AS_ERROR_FLAG -Werror)

set(OMR_ENHANCED_WARNING_FLAG -Wall)

list(APPEND OMR_PLATFORM_COMPILE_OPTIONS -pthread)

if(OMR_ENV_DATA64)
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
		-m64
	)
	list(APPEND OMR_PLATFORM_EXE_LINKER_OPTIONS
		-m64
	)
	list(APPEND OMR_PLATFORM_SHARED_LINKER_OPTIONS
		-m64
	)
else()
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
		-m32
		-msse2
	)
	list(APPEND OMR_PLATFORM_EXE_LINKER_OPTIONS
		-m32
	)
	list(APPEND OMR_PLATFORM_SHARED_LINKER_OPTIONS
		-m32
	)
endif()

if(OMR_HOST_ARCH STREQUAL "s390")
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS -march=z9-109)
endif()

# Testarossa build variables. Longer term the distinction between TR and the rest
# of the OMR code should be heavily reduced. In the mean time, we keep
# the distinction


# TR_COMPILE_OPTIONS are variables appended to CMAKE_{C,CXX}_FLAGS, and so
# apply to both C and C++ compilations.
list(APPEND TR_COMPILE_OPTIONS
	-Wno-write-strings #This warning swamps almost all other output
)

# TR_CXX_COMPILE_OPTIONS are appended to CMAKE_CXX_FLAGS, and so apply only to
# C++ file compilation
list(APPEND TR_CXX_COMPILE_OPTIONS
	-std=c++0x
)

# TR_C_COMPILE_OPTIONS are appended to CMAKE_C_FLAGS, and so apply only to
# C file compilation
list(APPEND TR_C_COMPILE_OPTIONS
)

set(PASM_CMD ${CMAKE_C_COMPILER})
set(PASM_FLAGS -x assembler-with-cpp -E -P)

set(SPP_CMD ${CMAKE_C_COMPILER})
set(SPP_FLAGS -x assembler-with-cpp -E -P)
