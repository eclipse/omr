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

omr_add_library(j9a2e SHARED
	sysTranslate.c
	atoe.c
	atoe_utils.c
)

target_compile_options(j9a2e
	PRIVATE
		${OMR_PLATFORM_SHARED_COMPILE_OPTIONS}
)

target_link_libraries(j9a2e
	PUBLIC
		omr_base
)

target_include_directories(j9a2e
	PUBLIC
		$<BUILD_INTERFACE:${OMR_SOURCE_DIR}/include_core>
		$<INSTALL_INTERFACE:${OMR_INSTALL_INC_DIR}>
	PRIVATE
		.
)

omr_process_exports(j9a2e)

install(
	TARGETS j9a2e
	EXPORT OmrTargets
	PUBLIC_HEADER DESTINATION ${OMR_INSTALL_INC_DIR}/
	ARCHIVE DESTINATION ${OMR_INSTALL_ARCHIVE_DIR}/
	LIBRARY DESTINATION ${OMR_INSTALL_LIB_DIR}/
)

set_property(TARGET j9a2e PROPERTY FOLDER util)


target_enable_ddr(j9a2e)
ddr_set_add_targets(omrddr j9a2e)
