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

include(OmrDDRSupport)

# We need to make sure that ddrgentest is built with debug info
# regardless of our global compile flags
omr_append_flags(CMAKE_C_FLAGS ${OMR_DEBUG_COMPILE_FLAG})
omr_append_flags(CMAKE_CXX_FLAGS ${OMR_DEBUG_COMPILE_FLAG})
omr_append_flags(CMAKE_EXE_LINKER_FLAGS ${OMR_DEBUG_LINK_FLAG})

add_executable(ddrgentest
    test_subdir/testFileInSubDir.cpp
    sample1.cpp
    sample2.cpp
    sample3.cpp
    sample4.c
    test.cpp
)

make_ddr_set(ddr_testset)
set_target_properties(ddr_testset PROPERTIES
    BLOB ${CMAKE_CURRENT_BINARY_DIR}/blob.dat
    SUPERSET ${CMAKE_CURRENT_BINARY_DIR}/superset.dat
    BLACKLIST ${CMAKE_CURRENT_SOURCE_DIR}/blacklist
    EXCLUDE_FROM_ALL FALSE
)
target_enable_ddr(ddrgentest GLOB_HEADERS_RECURSIVE)
ddr_set_add_targets(ddr_testset ddrgentest)
