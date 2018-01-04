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

#TODO need to port following makefile snippet
#ifeq ($(OMR_TOOLCHAIN),gcc)
#    MODULE_CFLAGS += -Wno-unused-value
#endif

add_tracegen(hashtable.tdf)

add_library(j9hashtable STATIC
	hash.c
	hashtable.c
	ut_hashtable.c
)

if(OMR_WARNINGS_AS_ERRORS)
	target_compile_options(j9hashtable PRIVATE ${OMR_WARNING_AS_ERROR_FLAG})
endif()

if(OMR_ENHANCED_WARNINGS)
	target_compile_options(j9hashtable PRIVATE ${OMR_ENHANCED_WARNING_FLAG})
endif()

target_include_directories(j9hashtable
	PUBLIC
		.
)

target_link_libraries(j9hashtable
	PUBLIC
		j9avl
		j9pool
		omrutil
)

set_property(TARGET j9hashtable PROPERTY FOLDER util)
