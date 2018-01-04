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

include_directories(${CMAKE_CURRENT_BINARY_DIR})

#TODO there is a bunch of stuff with vpaths, presumably for some extensibility reasons
# need to figure out if its actually needed and implement properly if required
add_library(j9hook_obj OBJECT
	${CMAKE_CURRENT_SOURCE_DIR}/hookable.cpp
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9hook.c
)

target_include_directories(j9hook_obj
	PUBLIC
		.
)

add_library(j9hookstatic STATIC
	$<TARGET_OBJECTS:j9hook_obj>
)

if(OMR_WARNINGS_AS_ERRORS)
	target_compile_options(j9hook_obj PRIVATE ${OMR_WARNING_AS_ERROR_FLAG})
	target_compile_options(j9hookstatic PRIVATE ${OMR_WARNING_AS_ERROR_FLAG})
endif()

if(OMR_ENHANCED_WARNINGS)
	target_compile_options(j9hook_obj PRIVATE ${OMR_ENHANCED_WARNING_FLAG})
	target_compile_options(j9hookstatic PRIVATE ${OMR_ENHANCED_WARNING_FLAG})
endif()

target_include_directories(j9hookstatic
	PUBLIC
	$<TARGET_PROPERTY:j9hook_obj,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(j9hookstatic PUBLIC
	j9thrstatic
	j9pool
	omrport
)
add_tracegen(j9hook.tdf)

set_target_properties(j9hook_obj j9hookstatic PROPERTIES FOLDER util)
