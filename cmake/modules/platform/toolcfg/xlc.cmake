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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

set(OMR_WARNING_AS_ERROR_FLAG -qhalt=w)

#There is no enhanced warning for XLC right now
set(OMR_ENHANCED_WARNING_FLAG )

list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
	-qalias=noansi
	-qxflag=LTOL:LTOL0
)

if(OMR_ENV_DATA64)
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
		-q64
	)
else()
	#-qarch should be there for 32 and 64 C/CXX flags but the C compiler is used for the assembler and it has trouble with some assembly files if it is specified
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
		-q32
		-qarch=ppc
	)
endif()


if(OMR_HOST_OS STREQUAL "aix")
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
		-qlanglvl=extended
		-qinfo=pro
	)

	set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lm -liconv -ldl -lperfstat")

	if(OMR_ENV_DATA64)
		set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -q64")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -q64")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -q64")

		set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> -X64 cr <TARGET> <LINK_FLAGS> <OBJECTS>")
		set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> -X64 cr <TARGET> <LINK_FLAGS> <OBJECTS>")
		set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> -X64 <TARGET>")
	endif()

elseif(OMR_HOST_OS STREQUAL "linux")
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS
		-qxflag=selinux
	)
endif()

# Testarossa build variables. Longer term the distinction between TR and the rest
# of the OMR code should be heavily reduced. In the mean time, we keep
# the distinction

# TR_COMPILE_OPTIONS are variables appended to CMAKE_{C,CXX}_FLAGS, and so
# apply to both C and C++ compilations.
list(APPEND TR_COMPILE_OPTIONS
	-qarch=pwr7
	-qtls
	-qnotempinc
	-qenum=small
	-qmbcs
	-qfuncsect
	-qsuppress=1540-1087:1540-1088:1540-1090:1540-029:1500-029
	-qdebug=nscrep
)

# TR_CXX_COMPILE_OPTIONS are appended to CMAKE_CXX_FLAGS, and so apply only to
# C++ file compilation
list(APPEND TR_CXX_COMPILE_OPTIONS
	-qlanglvl=extended0x
)

# TR_C_COMPILE_OPTIONS are appended to CMAKE_C_FLAGS, and so apply only to
# C file compilation
list(APPEND TR_C_COMPILE_OPTIONS
)

set(SPP_CMD ${CMAKE_C_COMPILER})
set(SPP_FLAGS -E -P)
