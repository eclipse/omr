###############################################################################
# Copyright (c) 2019, 2020 IBM Corp. and others
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

# This module locates a minimal set of cuda headers, required by the OMR_OPT_CUDA flag.
# This module will search for CUDA resources in only one directory. The variables tested, in order of preference, are:
#
# 1. The cmake variable OMR_CUDA_HOME, if set.
# 2. The cmake variable CUDA_HOME, if set.
# 3. The environment variable CUDA_HOME, if set.
#

include(FindPackageHandleStandardArgs)

set(OMR_CUDA_HOME "NOTFOUND" CACHE PATH "Path to the CUDA SDK. Takes precedence over CUDA_HOME in OMR.")
set(OmrCuda_INCLUDE_DIRS "OmrCuda_INCLUDE_DIRS-NOTFOUND")

if(OmrCuda_FIND_REQUIRED)
	set(error_level FATAL_ERROR)
else()
	set(error_level WARNING)
endif()

# Establish which directory we are searching in

set(OmrCuda_SEARCH_DIR "")

if(OMR_CUDA_HOME)
	set(OmrCuda_SEARCH_DIR "${OMR_CUDA_HOME}")
elseif(CUDA_HOME)
	set(OmrCuda_SEARCH_DIR "${CUDA_HOME}")
elseif(ENV{CUDA_HOME})
	set(OmrCuda_SEARCH_DIR "$ENV{CUDA_HOME}")
else()
	message(${error_level} "CUDA support requested, but OMR_CUDA_HOME/CUDA_HOME are not set.")
endif()

# Try to locate the main CUDA include directory by finding cuda.h

find_path(OmrCuda_INCLUDE_DIR
	NAMES cuda.h
	PATHS ${OmrCuda_SEARCH_DIR}
	PATH_SUFFIXES include Headers
	DOC "The CUDA include directory"
	NO_DEFAULT_PATH
)

find_package_handle_standard_args(OmrCuda
	DEFAULT_MSG
	OmrCuda_INCLUDE_DIR
)

if(OmrCuda_FOUND)
	set(OmrCuda_INCLUDE_DIRS "${OmrCuda_INCLUDE_DIR}")
endif()
