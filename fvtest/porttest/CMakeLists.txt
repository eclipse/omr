###############################################################################
# Copyright IBM Corp. and others 2017
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

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
)

if(OMR_OPT_CUDA)
	target_sources(omrporttest
		PRIVATE
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

# TODO: Remove if(NOT OMR_OS_WINDOWS) after OMRSOCK API is implemented on Windows.
if(NOT OMR_OS_WINDOWS)
	target_sources(omrporttest
		PRIVATE
			omrsockTest.cpp
	)
endif()

target_include_directories(omrporttest
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(omrporttest
	omrGtestGlue
	omrtestutil
	${OMR_PORT_LIB}
)

if(OMR_OS_WINDOWS)
	target_link_libraries(omrporttest
		ws2_32
	)
endif()

omr_add_library(sltestlib SHARED
	sltestlib/sltest.c
)

target_compile_options(sltestlib
	PRIVATE
		${OMR_PLATFORM_SHARED_COMPILE_OPTIONS}
)

set_target_properties(omrporttest sltestlib
	PROPERTIES
		FOLDER fvtest
)

omr_add_exports(sltestlib
	sl_test1_function
)

if(OMR_OS_AIX)
	omr_add_library(aixbaddep SHARED
		aixbaddep/sltest.c
	)

	target_compile_options(aixbaddep
		PRIVATE
			${OMR_PLATFORM_SHARED_COMPILE_OPTIONS}
	)

	set_target_properties(aixbaddep
		PROPERTIES
			LINK_FLAGS "-bI:${CMAKE_CURRENT_SOURCE_DIR}/aixbaddep/dummy.imp"
			FOLDER fvtest
	)

	omr_add_exports(aixbaddep
		sl_AixDLLMissingDependency_function
	)
endif()

if(OMR_OS_ZOS AND OMR_ENV_DATA64)
	omr_add_library(sltestlib31 SHARED
		sltestlib31/sltest.cpp
	)

	# Force 31-bit NON-XPLINK. These options appended at the end overrides
	# the default 64-bit options from CMAKE_C_FLAGS.
	target_compile_options(sltestlib31
		PRIVATE
			"-Wc,ILP32"    # Force AMODE31 generation
			"-Wc,noxplink" # Force OS Linkage
	)

	# Force AMODE31/OS linkage for linking as well. Unfortunately,
	# there are no -Wl,ILP32 -Wl,noxplink linker options and using LINK_FLAGS
	# does not append the options at the very end. However, specifying
	# -Wc,ILP32 and -Wc,noxplink, along with the *.o being AMODE31
	# will result in the binder to build an AMODE31 shared library.
	# This requires xlc/xlc++ to work. If we are able to use CMake 3.13
	# or newer on z/OS one day, we should directly update target_link_options
	# instead.
	set_target_properties(sltestlib31
		PROPERTIES
			LINK_FLAGS "-Wc,ILP32 -Wc,noxplink"  # Force AMODE31/OS Linkage
			FOLDER fvtest
	)

	omr_add_exports(sltestlib31
		sl_testOpen31bitDLLviaCEL4RO31_function
	)
endif()

if(OMR_OS_AIX)
	set(EXCLUDE_LIST "${EXCLUDE_LIST}-PortSockTest.two_socket_datagram_communication")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSlTest.sl_AixDLLMissingDependency")
elseif(OMR_OS_ZOS)
	set(EXCLUDE_LIST "${EXCLUDE_LIST}-PortFileTest2.file_test20")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileTest2.file_test21")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileTest2.file_test22")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileTest2.file_test23")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileTest2.file_test24")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileTest2.file_test25")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileTest2.file_test27")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileStreamTest.omrfilestream_test_text")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileStreamTest.omrfilestream_test_omrfile_text_comparison")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortMmapTest.mmap_test5")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSigTest.sig_test_async_handler")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSigTest.sig_test_single_async_handler")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSigTest.sig_test_mix_async_handler")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSigTest.sig_test_mix_two_async_handlers")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortStrTest.str_test2")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortStrTest.str_convPlatTo8")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortStrTest.str_convU8ToPlat")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortStrTest.str_convU8ToWide")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortStrTest.str_convWideToU8")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortStrTest.str_convU8ToPlat_Null")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSysinfoTest.sysinfo_test_get_cwd3")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSockTest.two_socket_datagram_communication")
elseif(OMR_OS_LINUX AND OMR_ARCH_RISCV AND CMAKE_CROSSCOMPILING)
	set(EXCLUDE_LIST "${EXCLUDE_LIST}-PortFileTest2.file_test20")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileTest2.file_test21")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileTest2.file_test22")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileTest2.file_test23")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileTest2.file_test24")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortFileTest2.file_test25")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortMmapTest.mmap_test5")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSigTest.sig_test_async_handler")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSigTest.sig_test_single_async_handler")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSigTest.sig_test_mix_async_handler")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSigTest.sig_test_mix_two_async_handlers")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortStrTest.str_test4")
	set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortSysinfoTest.sysinfo_test_sysinfo_set_limit_ADDRESS_SPACE")
elseif(OMR_OS_LINUX AND OMR_ENV_DATA32)
	set(EXCLUDE_LIST "${EXCLUDE_LIST}-PortVmemTest.vmem_test_reserveExecutableMemory")
elseif(OMR_OS_OSX AND OMR_ARCH_AARCH64)
	set(EXCLUDE_LIST "${EXCLUDE_LIST}-PortMemTest.mem_test7_allocate32")
endif()

if($ENV{OMR_RUNNING_IN_DOCKER})
	if(EXCLUDE_LIST)
		set(EXCLUDE_LIST "${EXCLUDE_LIST}:PortStrTest.str_test4")
	else()
		set(EXCLUDE_LIST "${EXCLUDE_LIST}-PortStrTest.str_test4")
	endif()
endif()

if(EXCLUDE_LIST)
	set(EXCLUDE_LIST "--gtest_filter=${EXCLUDE_LIST}")
	message(WARNING "Excluded omrport tests: ${EXCLUDE_LIST}")
endif()

omr_add_test(
	NAME porttest
	COMMAND $<TARGET_FILE:omrporttest> --gtest_output=xml:${CMAKE_CURRENT_BINARY_DIR}/omrporttest-results.xml ${EXCLUDE_LIST}
)

if(OMR_OPT_CUDA)
	omr_add_test(
		NAME cuda_porttest
		COMMAND $<TARGET_FILE:omrporttest> --gtest_output=xml:${CMAKE_CURRENT_BINARY_DIR}/omrporttest-results.xml --gtest_filter=*Cuda* -earlyExit
	)
endif()
