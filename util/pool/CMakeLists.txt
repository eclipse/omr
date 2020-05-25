###############################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_tracegen(pool.tdf)

omr_add_library(j9pool STATIC
	pool.c
	pool_cap.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_pool.c
)

target_link_libraries(j9pool
	PUBLIC
		omr_base
)

set_property(TARGET j9pool PROPERTY FOLDER util)

target_enable_ddr(j9pool)
ddr_add_headers(j9pool ${omr_SOURCE_DIR}/include_core/j9pool.h)
ddr_set_add_targets(omrddr j9pool)
