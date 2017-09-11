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

include(../../cmake/compiler_support.cmake) 

set(COMPILERTEST_FILES
	ilgen/BinaryOpIlInjector.cpp
	ilgen/ChildlessUnaryOpIlInjector.cpp
	ilgen/CmpBranchOpIlInjector.cpp
	ilgen/OpIlInjector.cpp
	ilgen/StoreOpIlInjector.cpp
	ilgen/TernaryOpIlInjector.cpp
	ilgen/UnaryOpIlInjector.cpp
	compile/Method.cpp
	control/TestJit.cpp
	env/FrontEnd.cpp
	ilgen/IlInjector.cpp
	ilgen/TestIlGeneratorMethodDetails.cpp
	optimizer/TestOptimizer.cpp
	runtime/TestCodeCacheManager.cpp
	runtime/TestJitConfig.cpp
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
		${OMR_ROOT}/compiler/p/runtime/OMRCodeCacheConfig.cpp
#target
		p/codegen/Evaluator.cpp
		${OMR_ROOT}/compiler/p/env/OMRDebugEnv.cpp
	)
endif()

create_omr_compiler_library(NAME       testcompiler 
                            OBJECTS    ${COMPILERTEST_FILES}
							DEFINES JITTEST TEST_PROJECT_SPECIFIC PROD_WITH_ASSUMES)
add_executable(compilertest
	tests/main.cpp
	tests/BarIlInjector.cpp
	tests/BuilderTest.cpp
	tests/CallIlInjector.cpp
	tests/IndirectLoadIlInjector.cpp
	tests/IndirectStoreIlInjector.cpp
	tests/FooBarTest.cpp
	tests/FooIlInjector.cpp
	tests/LimitFileTest.cpp
	tests/LogFileTest.cpp
	tests/OMRTestEnv.cpp
	tests/OptionSetTest.cpp
	tests/OpCodesTest.cpp
	tests/PPCOpCodesTest.cpp
	tests/Qux2Test.cpp
	tests/Qux2IlInjector.cpp
	tests/SimplifierFoldAndTest.cpp
	tests/S390OpCodesTest.cpp
	tests/OptTestDriver.cpp
	tests/TestDriver.cpp
	tests/X86OpCodesTest.cpp

)


# Inherit the include path from the compiler component
make_compiler_target(compilertest COMPILER testcompiler)

# target_include_directories(compilertest PRIVATE 
# 	${PROJECT_SOURCE_DIR}/third_party/gtest-1.7.0/
# 	${PROJECT_SOURCE_DIR}/third_party/gtest-1.7.0/include
# )

target_link_libraries(compilertest
	testcompiler	
	omrGtest
    ${CMAKE_DL_LIBS}
)

add_test(NAME CompilerTest COMMAND compilertest) 
