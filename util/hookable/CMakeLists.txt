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

#TODO there is a bunch of stuff with vpaths, presumably for some extensibility reasons
# need to figure out if its actually needed and implement properly if required
omr_add_library(j9hook_obj OBJECT
	${CMAKE_CURRENT_SOURCE_DIR}/hookable.cpp
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9hook.c
)

target_include_directories(j9hook_obj
	PUBLIC
		.
)

target_include_directories(j9hook_obj
	PUBLIC
		$<TARGET_PROPERTY:omr_base,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(j9hook_obj
	PUBLIC
		$<TARGET_PROPERTY:omr_base,INTERFACE_COMPILE_DEFINITIONS>
)

omr_add_exports(j9hook_obj
	J9HookInitializeInterface
	omrhook_lib_control
)

target_enable_ddr(j9hook_obj)
ddr_set_add_targets(omrddr j9hook_obj)

omr_add_library(j9hookstatic STATIC
	$<TARGET_OBJECTS:j9hook_obj>
)
# Duplicate the export list onto the static library version
get_target_property(hook_exports j9hook_obj EXPORTED_SYMBOLS)
omr_add_exports(j9hookstatic ${hook_exports})

target_include_directories(j9hookstatic
	PUBLIC
	$<TARGET_PROPERTY:j9hook_obj,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(j9hookstatic
	PUBLIC
		omr_base
		${OMR_PORT_LIB}
		j9pool
)
add_tracegen(j9hook.tdf)

set_target_properties(j9hook_obj j9hookstatic PROPERTIES FOLDER util)
