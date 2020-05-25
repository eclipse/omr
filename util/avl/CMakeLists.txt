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

add_tracegen(avl.tdf)

set(j9avl_public_headers
	"${omr_SOURCE_DIR}/include_core/avl_api.h"
)

omr_add_library(j9avl STATIC
	avlsup.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_avl.c
)

target_link_libraries(j9avl
	PUBLIC
		omr_base
)

target_include_directories(j9avl
	PUBLIC
		$<BUILD_INTERFACE:${omr_SOURCE_DIR}/include_core>
		$<INSTALL_INTERFACE:${OMR_INSTALL_INC_DIR}>
	PRIVATE
		.
		${CMAKE_CURRENT_BINARY_DIR}
)

set_property(TARGET j9avl PROPERTY FOLDER util)

set_target_properties(j9avl PROPERTIES PUBLIC_HEADER ${j9avl_public_headers})

install(
	TARGETS j9avl
	EXPORT OmrTargets
	PUBLIC_HEADER DESTINATION ${OMR_INSTALL_INC_DIR}/
	ARCHIVE DESTINATION ${OMR_INSTALL_ARCHIVE_DIR}/
	LIBRARY DESTINATION ${OMR_INSTALL_LIB_DIR}/
)

target_enable_ddr(j9avl)
ddr_add_headers(j9avl
	${omr_SOURCE_DIR}/include_core/omravl.h
	${omr_SOURCE_DIR}/include_core/omravldefines.h
)
ddr_set_add_targets(omrddr j9avl)
