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

# Find DIA SDK
# Will search the environment variable DIASDK first.
#
# Will set:
#   DIASDK_FOUND
#   DIASDK_INCLUDE_DIRS
#   DIASDK_LIBRARIES
#   DIASDK_DEFINITIONS

find_path(DIA2_H_DIR "dia2.h"
	HINTS
		"$ENV{DIASDK}\\include"
		"$ENV{VSSDK140Install}..\\DIA SDK\\include"
)

find_library(DIAGUIDS_LIBRARY "diaguids"
	HINTS
		"$ENV{DIASDK}\\lib"
		"$ENV{VSSDK140Install}..\\DIA SDK\\lib"
)

include (FindPackageHandleStandardArgs)

find_package_handle_standard_args(DiaSDK
	DEFAULT_MSG
	DIAGUIDS_LIBRARY
	DIA2_H_DIR
)

if(DIASDK_FOUND)
	set(DIASDK_DEFINITIONS -DHAVE_DIA)
	set(DIASDK_INCLUDE_DIRS ${DIA2_H_DIR})
	set(DIASDK_LIBRARIES ${DIAGUIDS_LIBRARY})
endif(DIASDK_FOUND)

