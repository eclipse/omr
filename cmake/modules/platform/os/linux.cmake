###############################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
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

include(CheckSymbolExists)

list(APPEND OMR_PLATFORM_DEFINITIONS
	-DLINUX
	-D_FILE_OFFSET_BITS=64
)

if(OMR_HOST_ARCH STREQUAL "ppc")
	if(OMR_ENV_DATA64)
		list(APPEND OMR_PLATFORM_DEFINITIONS
			-DLINUXPPC64
		)
	endif()
	list(APPEND OMR_PLATFORM_DEFINITIONS
		-DLINUXPPC
	)
elseif(OMR_HOST_ARCH STREQUAL "s390")
	if(OMR_ENV_DATA64)
		list(APPEND OMR_PLATFORM_DEFINITIONS
			-DS39064
		)
	endif()

	list(APPEND OMR_PLATFORM_DEFINITIONS
		-DS390
	)
endif()

# Check if OMR needs to use librt. This variable is used by containing projects.
# to properly link against OMR.
if(NOT DEFINED OMR_NEED_LIBRT)
	check_symbol_exists(clock_gettime time.h OMR_LIBC_HAS_CLOCK_GETTIME)
	if(OMR_LIBC_HAS_CLOCK_GETTIME)
		set(OMR_NEED_LIBRT FALSE CACHE BOOL "")
	else()
		set(OMR_NEED_LIBRT TRUE CACHE BOOL "")
		set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lrt")
	endif()
	mark_as_advanced(OMR_NEED_LIBRT)
endif()

# Testarossa build variables. Longer term the distinction between TR and the rest
# of the OMR code should be heavily reduced. In the mean time, we keep the distinction.
list(APPEND TR_COMPILE_DEFINITIONS -DSUPPORTS_THREAD_LOCAL -DLINUX)
