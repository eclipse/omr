###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
# 
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
# 
# This Source Code is also Distributed under one or more Secondary Licenses,
# as those terms are defined by the Eclipse Public License, v. 2.0: GNU
# General Public License, version 2 with the GNU Classpath Exception [1]
# and GNU General Public License, version 2 with the OpenJDK Assembly
# Exception [2].
# 
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
# 
# Contributors:
#   Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################

add_library(j9a2e SHARED
	sysTranslate.c
	atoe.c
	atoe_utils.c
)

target_link_libraries(j9a2e PRIVATE omr_shared)

target_include_directories(j9a2e
	PUBLIC
		.
)

add_custom_command(TARGET j9a2e
	POST_BUILD
	COMMAND mv j9a2e.x ${CMAKE_BINARY_DIR}/j9a2e.x
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

