###############################################################################
# Copyright IBM Corp. and others 2017
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
###############################################################################

include(OmrCompilerSupport)

find_package(PythonInterp REQUIRED)

# JitBuilder Files
set(JITBUILDER_OBJECTS
	env/FrontEnd.cpp
	compile/ResolvedMethod.cpp
	control/Jit.cpp
	ilgen/JBIlGeneratorMethodDetails.cpp
	optimizer/JBOptimizer.hpp
	optimizer/JBOptimizer.cpp
	optimizer/Optimizer.hpp
	runtime/JBCodeCacheManager.cpp
	runtime/JBJitConfig.cpp
)

if(OMR_ARCH_X86)
	list(APPEND JITBUILDER_OBJECTS
		x/codegen/Evaluator.cpp
	)
elseif(OMR_ARCH_S390)
	list(APPEND JITBUILDER_OBJECTS
		z/codegen/Evaluator.cpp
	)
elseif(OMR_ARCH_POWER)
	list(APPEND JITBUILDER_OBJECTS
		p/codegen/Evaluator.cpp
		p/runtime/CodeSync.cpp
		p/runtime/AsmUtil.spp
		p/runtime/CodeDispatch.spp
		${omr_SOURCE_DIR}/compiler/p/env/OMRDebugEnv.cpp
	)
endif()

# Generated client API files
set(CPP_API_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/apigen/cppgen.py)
set(JITBUILDER_CPP_API_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/release/cpp/include)
set(JITBUILDER_CPP_API_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}client/cpp)
set(JITBUILDER_API_DESCRIPTION ${CMAKE_CURRENT_SOURCE_DIR}/apigen/jitbuilder.api.json)

set(JITBUILDER_API_SOURCES
	${JITBUILDER_CPP_API_SOURCE_DIR}/BytecodeBuilder.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/IlBuilder.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/IlType.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/IlValue.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/MethodBuilder.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/ThunkBuilder.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/TypeDictionary.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/VirtualMachineOperandArray.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/VirtualMachineOperandStack.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/VirtualMachineRegister.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/VirtualMachineRegisterInStruct.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/VirtualMachineState.cpp
	${JITBUILDER_CPP_API_SOURCE_DIR}/JitBuilder.cpp
)

set(JITBUILDER_API_HEADERS
	${JITBUILDER_CPP_API_HEADER_DIR}/BytecodeBuilder.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/IlBuilder.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/IlType.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/IlValue.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/MethodBuilder.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/ThunkBuilder.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/TypeDictionary.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/VirtualMachineOperandArray.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/VirtualMachineOperandStack.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/VirtualMachineRegister.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/VirtualMachineRegisterInStruct.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/VirtualMachineState.hpp
	${JITBUILDER_CPP_API_HEADER_DIR}/JitBuilder.hpp
)

add_custom_command(
	OUTPUT ${JITBUILDER_API_SOURCES} ${JITBUILDER_API_HEADERS}
	COMMAND ${CMAKE_COMMAND} -E make_directory ${JITBUILDER_CPP_API_SOURCE_DIR}
	COMMAND ${CMAKE_COMMAND} -E make_directory ${JITBUILDER_CPP_API_HEADER_DIR}
	COMMAND ${PYTHON_EXECUTABLE} ${CPP_API_GENERATOR} ${JITBUILDER_API_DESCRIPTION} --sourcedir ${JITBUILDER_CPP_API_SOURCE_DIR} --headerdir ${JITBUILDER_CPP_API_HEADER_DIR}
	DEPENDS ${CPP_API_GENERATOR} ${JITBUILDER_API_DESCRIPTION}
	COMMENT "Running JitBuilder C++ API generator"
)

list(APPEND JITBUILDER_OBJECTS
	${JITBUILDER_API_SOURCES}
)

if(OMR_ARCH_X86 OR OMR_ARCH_S390)
	set(OMR_ENHANCED_WARNINGS OFF)
else()
	set(OMR_WARNINGS_AS_ERRORS OFF)
	set(OMR_ENHANCED_WARNINGS OFF)
endif()

# Workaround for deprecated sprintf
if(OMR_OS_OSX)
	set(OMR_WARNINGS_AS_ERRORS OFF)
endif()

# Create jitbuilder library.
create_omr_compiler_library(
	NAME    jitbuilder
	OBJECTS ${JITBUILDER_OBJECTS}
	DEFINES PROD_WITH_ASSUMES JITTEST
)

# Add interface path so that include paths propagate.
# NOTE: `release` directory isn't being automatically setup, so this
#       is adding the actual compiler dir, as opposed to the 'composed'
#       install directory.
target_include_directories(jitbuilder
	INTERFACE
		${JITBUILDER_CPP_API_HEADER_DIR}
)

target_link_libraries(jitbuilder
	PUBLIC
		${OMR_PORT_LIB}
)

# JitBuilder examples only work on 64 bit currently.
if(OMR_JITBUILDER AND OMR_ENV_DATA64)
	add_subdirectory(release)
endif()

if(OMR_JITBUILDER_TEST AND NOT (OMR_OS_OSX AND OMR_ARCH_X86))
	add_test(
		NAME TestJitBuilderAPIGenerator
		COMMAND ${PYTHON_EXECUTABLE} run_tests.py
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/apigen
	)
endif()

if(OMR_LLJB)
	add_subdirectory(lljb)
endif()

# install(TARGETS jitbuilder
#         ARCHIVE       DESTINATION ${CMAKE_BINARY_DIR}/jitbuilder_release
#         PUBLIC_HEADER DESTINATION ${CMAKE_BINARY_DIR}/jitbuilder_release/include
#         )
#
# # Create release directory
# file(MAKE_DIRECTORY jitbuilder_release)
# file(COPY ${jitbuilder_library} DESTINATION jitbuilder_release)
# file(COPY ${CMAKE_SOURCE_DIR}/compiler/env   DESTINATION jitbuilder_release/include)
# file(COPY ${CMAKE_SOURCE_DIR}/compiler/il    DESTINATION jitbuilder_release/include)
# file(COPY ${CMAKE_SOURCE_DIR}/compiler/ilgen DESTINATION jitbuilder_release/include)
