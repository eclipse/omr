###############################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
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

include(OmrCompilerSupport)

set(COMPILERTEST_FILES
	compile/Method.cpp
	control/TestJit.cpp
	env/FrontEnd.cpp
	ilgen/IlInjector.cpp
	ilgen/TestIlGeneratorMethodDetails.cpp
	runtime/TestCodeCacheManager.cpp
)

if(OMR_ARCH_X86)
	list(APPEND COMPILERTEST_FILES
		x/codegen/Evaluator.cpp
	)
elseif(OMR_ARCH_POWER)
	list(APPEND COMPILERTEST_FILES
		#Host
		p/runtime/AsmUtil.spp
		p/runtime/CodeDispatch.spp
		p/runtime/CodeSync.cpp
		${omr_SOURCE_DIR}/compiler/p/runtime/OMRCodeCacheConfig.cpp
		#target
		p/codegen/Evaluator.cpp
		${omr_SOURCE_DIR}/compiler/p/env/OMRDebugEnv.cpp
	)
elseif(OMR_ARCH_S390)
	list(APPEND COMPILERTEST_FILES 
		z/codegen/Evaluator.cpp
		z/codegen/TestCodeGenerator.cpp
		${omr_SOURCE_DIR}/compiler/z/env/OMRCPU.cpp
		${omr_SOURCE_DIR}/compiler/z/env/OMRDebugEnv.cpp
	)
endif()

create_omr_compiler_library(
	NAME testcompiler
	OBJECTS ${COMPILERTEST_FILES}
	DEFINES
		JITTEST
		TEST_PROJECT_SPECIFIC
		PROD_WITH_ASSUMES
		INCLUDES ${omr_SOURCE_DIR}/fvtest
)

# Export paths for dependent objects
make_compiler_target(testcompiler INTERFACE COMPILER testcompiler)

add_executable(compilertest
	tests/main.cpp
	tests/BuilderTest.cpp
	tests/FooBarTest.cpp
	tests/LimitFileTest.cpp
	tests/LogFileTest.cpp
	tests/OMRTestEnv.cpp
	tests/OptionSetTest.cpp
	tests/OpCodesTest.cpp
	tests/Qux2Test.cpp
	tests/SimplifierFoldAndTest.cpp
	tests/OptTestDriver.cpp
	tests/TestDriver.cpp
	tests/SingleBitContainerTest.cpp
	tests/injectors/BarIlInjector.cpp
	tests/injectors/BinaryOpIlInjector.cpp
	tests/injectors/CallIlInjector.cpp
	tests/injectors/ChildlessUnaryOpIlInjector.cpp
	tests/injectors/CmpBranchOpIlInjector.cpp
	tests/injectors/FooIlInjector.cpp
	tests/injectors/IndirectLoadIlInjector.cpp
	tests/injectors/IndirectStoreIlInjector.cpp
	tests/injectors/OpIlInjector.cpp
	tests/injectors/Qux2IlInjector.cpp
	tests/injectors/StoreOpIlInjector.cpp
	tests/injectors/TernaryOpIlInjector.cpp
	tests/injectors/UnaryOpIlInjector.cpp
)

if(OMR_ARCH_X86)
	target_sources(compilertest
		PRIVATE
			tests/X86OpCodesTest.cpp
	)
elseif(OMR_ARCH_POWER)
	target_sources(compilertest
		PRIVATE
			tests/PPCOpCodesTest.cpp
	)
elseif(OMR_ARCH_S390)
	target_sources(compilertest
		PRIVATE
			tests/S390OpCodesTest.cpp
	)
elseif(OMR_ARCH_AARCH64)
	target_sources(compilertest
		PRIVATE
			tests/ARM64OpCodesTest.cpp
	)
endif()

target_link_libraries(compilertest
	testcompiler
	omrGtest
	${CMAKE_DL_LIBS}
	${OMR_PORT_LIB}
)

set_property(TARGET compilertest PROPERTY FOLDER fvtest)

add_test(NAME CompilerTest COMMAND compilertest --gtest_output=xml:${CMAKE_CURRENT_BINARY_DIR}/compilertest-results.xml)
