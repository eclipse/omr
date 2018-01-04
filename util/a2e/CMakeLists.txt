###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

add_library(j9a2e SHARED
	sysTranslate.c
	atoe.c
	atoe_utils.c
)

target_compile_options(j9a2e
	PRIVATE
		${OMR_PLATFORM_SHARED_COMPILE_OPTIONS}
)

if(OMR_WARNINGS_AS_ERRORS)
	target_compile_options(j9a2e PRIVATE ${OMR_WARNING_AS_ERROR_FLAG})
endif()

if(OMR_ENHANCED_WARNINGS)
	target_compile_options(j9a2e PRIVATE ${OMR_ENHANCED_WARNING_FLAG})
endif()

target_include_directories(j9a2e
	PUBLIC
		.
)

add_custom_command(TARGET j9a2e
	POST_BUILD
	COMMAND mv j9a2e.x ${CMAKE_BINARY_DIR}/j9a2e.x
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_property(TARGET j9a2e PROPERTY FOLDER util)
