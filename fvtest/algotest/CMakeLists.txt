###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

omr_add_hookgen(INPUT hooksample.hdf)

add_executable(omralgotest
	algoTest.cpp
	algorithm_test_internal.h
	avltest.c
	avltest.lst
	hashtabletest.c
	hooksample.h
	hooksample_internal.h
	hooktest.c
	main.cpp
	pooltest.c

	# We need to introduce dependencies on the hookgen step.
	"${CMAKE_CURRENT_BINARY_DIR}/hooksample.h"
)

target_link_libraries(omralgotest
	omrGtestGlue
	omrport
	omrtestutil
	omrcore
	${OMR_GC_LIB}
)

target_include_directories(omralgotest PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

#TODO hack to get building
if(OMR_HOST_OS STREQUAL "win")
	target_link_libraries(omralgotest
		ws2_32
		shell32
		Iphlpapi
		psapi
		pdh
	)
endif()

target_link_libraries(omralgotest
	j9hookstatic
)

if(OMR_HOST_OS STREQUAL "zos")
	target_link_libraries(omralgotest j9a2e)
endif()

set_property(TARGET omralgotest PROPERTY FOLDER fvtest)

add_test(NAME algotest COMMAND omralgotest -avltest:${CMAKE_CURRENT_SOURCE_DIR}/avltest.lst)
