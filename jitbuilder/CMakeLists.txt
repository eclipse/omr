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

include(OmrCompilerSupport)


# JitBuilder Files
set(JITBUILDER_OBJECTS
	env/FrontEnd.cpp
	compile/Method.cpp
	control/Jit.cpp
	ilgen/JBIlGeneratorMethodDetails.cpp
	optimizer/JBOptimizer.hpp
	optimizer/JBOptimizer.cpp
	optimizer/Optimizer.hpp
	runtime/JBCodeCacheManager.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/VirtualMachineOperandArray.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/VirtualMachineOperandStack.cpp
)

if(OMR_ARCH_X86)
	list(APPEND JITBUILDER_OBJECTS
		x/codegen/Evaluator.cpp
	)
elseif(OMR_ARCH_S390)
	list(APPEND JITBUILDER_OBJECTS
		z/codegen/Evaluator.cpp
		z/codegen/JBCodeGenerator.cpp
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


# Create jitbuilder library.
create_omr_compiler_library(
	NAME    jitbuilder
	OBJECTS ${JITBUILDER_OBJECTS}
	DEFINES PROD_WITH_ASSUMES JITTEST
)

# Add interface path so that include paths propagate.
# NOTE: `release` directory  isn't being automatically setup, so this
#       is adding the actual compiler dir, as opposed to the 'composed'
#       install directory.
target_include_directories(jitbuilder
	INTERFACE
		release/include
		../compiler/
		../
)


add_subdirectory(release)

# install(TARGETS jitbuilder
#         ARCHIVE       DESTINATION ${CMAKE_BINARY_DIR}/jitbuilder_release
#         PUBLIC_HEADER DESTINATION ${CMAKE_BINARY_DIR}/jitbuilder_release/include
#         )
#
#
# #Create release directory
# file(MAKE_DIRECTORY jitbuilder_release)
# file(COPY ${jitbuilder_library} DESTINATION jitbuilder_release)
# file(COPY ${CMAKE_SOURCE_DIR}/compiler/env   DESTINATION jitbuilder_release/include)
# file(COPY ${CMAKE_SOURCE_DIR}/compiler/il    DESTINATION jitbuilder_release/include)
# file(COPY ${CMAKE_SOURCE_DIR}/compiler/ilgen DESTINATION jitbuilder_release/include)
