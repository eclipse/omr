###############################################################################
#
# (c) Copyright IBM Corp. 2017
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

set(cudaSources "")

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


add_executable(omrporttest
	fileTest.cpp
	heapTest.cpp
	j9portTest.cpp
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
	vmemTest.cpp
	
	${cudaSources}
)

omr_set_target_flags(omrporttest)

target_link_libraries(omrporttest
    PRIVATE
	omrGtestGlue
	omrcore
	omrport
	omrtestutil
)
if(OMR_HOST_OS STREQUAL "win")
	target_link_libraries(omrporttest
        PRIVATE
		ws2_32
		omrglue
	)
endif()

if(OMR_HOST_OS STREQUAL "zos")
	target_link_libraries(omrporttest PRIVATE j9a2e)
endif()


add_library(sltestlib SHARED
	sltestlib/sltest.c)

target_compile_options(sltestlib PRIVATE ${OMR_EXPORT_ALL})


add_test(NAME porttest COMMAND omrporttest)

if(OMR_OPT_CUDA)
	add_test(NAME cuda_porttest COMMAND omrporttest --gtest_filter="Cuda*" -earlyExit)
endif()

