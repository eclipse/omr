###############################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
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

# Find DIA SDK
# Will search the environment variable DIASDK first.
#
# Will set:
#   DIASDK_FOUND
#   DIASDK_INCLUDE_DIRS
#   DIASDK_LIBRARIES
#   DIASDK_DEFINITIONS

# Use the DiaSDK from the compiler if you are using Visual Studio.
# This works for a default install of VS2012.  As others are tested
# we may be able to rely on it for all machines. Currently I will
# leave the other hints as well.
get_filename_component(VSPath ${CMAKE_CXX_COMPILER} DIRECTORY CACHE)

# Note we need to tell CMake to search DiaSDK_ROOT
# this is default behavior on newer versions of cmake
find_path(DIA2_H_DIR "dia2.h"
	HINTS
		"${VSPath}/../../../DIA SDK/include"
		"$ENV{DIASDK}/include"
		"$ENV{VSSDK140Install}../DIA SDK/include"
		"${DiaSDK_ROOT}/include"
)

if(OMR_ENV_DATA64)
	set(lib_dir "lib/amd64")
else()
	set(lib_dir "lib")
endif()
find_library(DIAGUIDS_LIBRARY "diaguids"
	HINTS
		"${VSPath}/../../../DIA SDK/${lib_dir}"
		"$ENV{DIASDK}/${lib_dir}"
		"$ENV{VSSDK140Install}../DIA SDK/${lib_dir}"
		"${DiaSDK_ROOT}/${lib_dir}"
)

include (FindPackageHandleStandardArgs)

find_package_handle_standard_args(DiaSDK
	DEFAULT_MSG
	DIAGUIDS_LIBRARY
	DIA2_H_DIR
)

if(NOT DIASDK_FOUND)
	set(DIASDK_DEFINITIONS NOTFOUND)
	set(DIASDK_INCLUDE_DIRS NOTFOUND)
	set(DIASDK_LIBRARIES NOTFOUND)
	return()
endif()

# Everything below is only set if the library is found

set(DIASDK_DEFINITIONS -DHAVE_DIA)
set(DIASDK_INCLUDE_DIRS ${DIA2_H_DIR})
set(DIASDK_LIBRARIES ${DIAGUIDS_LIBRARY})

if(NOT TARGET DiaSDK::dia)
	add_library(DiaSDK::diasdk UNKNOWN IMPORTED)
	set_target_properties(DiaSDK::diasdk
		PROPERTIES
			IMPORTED_LOCATION "${DIAGUIDS_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${DIASDK_INCLUDE_DIRS}"
			INTERFACE_COMPILE_DEFINITIONS "${DIASDK_DEFINITIONS}"
	)
endif()
