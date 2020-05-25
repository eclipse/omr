###############################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
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
###############################################################################

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

set(cudaSources "")
set(socketSources "")

if(OMR_OPT_CUDA)
	set(cudaSources
		cudaBasic.cpp
		cudaError.cpp
		cudaEvent.cpp
		cudaInvalid.cpp
		cudaLaunch.cpp
		cudaLink.cpp
		cudaMemory.cpp
		cudaModule.cpp
		cudaPeer.cpp
		cudaProperty.cpp
		cudaPtx.cpp
		cudaStream.cpp
		cudaTests.cpp
	)
endif()

# TODO: Remove AND (NOT OMR_HOST_OS STREQUAL "win") after OMRSOCK API is implemented on Windows.
if((OMR_PORT_SOCKET_SUPPORT) AND (NOT OMR_HOST_OS STREQUAL "win"))
	set(socketSources
		omrsockTest.cpp
	)
endif()

omr_add_executable(omrporttest
	fileTest.cpp
	heapTest.cpp
	omrportTest.cpp
	main.cpp
	memTest.cpp
	omrdumpTest.cpp
	omrerrorTest.cpp
	omrfileTest.cpp
	omrfilestreamTest.cpp
	omrheapTest.cpp
	omrintrospectTest.cpp
	omrmemTest.cpp
	omrmmapTest.cpp
	omrsignalExtendedTest.cpp
	omrsignalTest.cpp
	omrslTest.cpp
	omrstrTest.cpp
	omrtimeTest.cpp
	omrttyExtendedTest.cpp
	omrttyTest.cpp
	omrvmemTest.cpp
	si.cpp
	si_numcpusTest.cpp
	testHelpers.cpp
	testProcessHelpers.cpp

	${cudaSources}
	${socketSources}
)



target_link_libraries(omrporttest
	omrGtestGlue
	omrtestutil
	${OMR_PORT_LIB}
)
if(OMR_HOST_OS STREQUAL "win")
	target_link_libraries(omrporttest
		ws2_32
	)
endif()

if(OMR_HOST_OS STREQUAL "zos")
	target_link_libraries(omrporttest j9a2e)
endif()

omr_add_library(sltestlib SHARED
	sltestlib/sltest.c
)

omr_add_exports(sltestlib
	sl_test1_function
)

target_compile_options(sltestlib
	PRIVATE
		${OMR_PLATFORM_SHARED_COMPILE_OPTIONS}
)

if(OMR_HOST_OS STREQUAL "aix")
	omr_add_library(aixbaddep SHARED
		aixbaddep/sltest.c
	)

	target_compile_options(aixbaddep
		PRIVATE
			${OMR_PLATFORM_SHARED_COMPILE_OPTIONS}
	)
	
	set_target_properties(aixbaddep PROPERTIES LINK_FLAGS "-bI:${CMAKE_CURRENT_SOURCE_DIR}/aixbaddep/dummy.imp" FOLDER fvtest)
	omr_add_exports(aixbaddep
		sl_AixDLLMissingDependency_function
	)
endif()

set_target_properties(omrporttest sltestlib PROPERTIES FOLDER fvtest)

if (NOT OMR_HOST_OS STREQUAL "aix" AND NOT OMR_HOST_OS STREQUAL "zos")
	add_test(NAME porttest COMMAND omrporttest --gtest_output=xml:${CMAKE_CURRENT_BINARY_DIR}/omrporttest-results.xml)
endif()

if(OMR_OPT_CUDA)
	add_test(NAME cuda_porttest COMMAND omrporttest --gtest_output=xml:${CMAKE_CURRENT_BINARY_DIR}/omrporttest-results.xml --gtest_filter="Cuda*" -earlyExit)
endif()
