###############################################################################
# Copyright IBM Corp. and others 2017
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
#############################################################################

compiler_library(x
	${CMAKE_CURRENT_LIST_DIR}/codegen/BinaryCommutativeAnalyser.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/BinaryEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/CompareAnalyser.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/ControlFlowEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/DataSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/DivideCheckSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/FPBinaryArithmeticAnalyser.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/FPCompareAnalyser.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/FPTreeEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/SIMDTreeEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/HelperCallSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/IA32LinkageUtils.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/IntegerMultiplyDecomposer.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRMemoryReference.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRInstOpCode.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OutlinedInstructions.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/RegisterRematerialization.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/SubtractAnalyser.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRTreeEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/UnaryEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/X86BinaryEncoding.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/X86Debug.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/X86FPConversionSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRAheadOfTimeCompile.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRInstruction.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRInstructionDelegate.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRX86Instruction.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRMachine.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRLinkage.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRRegister.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRRealRegister.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRRegisterDependency.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRSnippetDelegate.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/X86SystemLinkage.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRCodeGenerator.cpp
	${CMAKE_CURRENT_LIST_DIR}/env/OMRCPU.cpp
	${CMAKE_CURRENT_LIST_DIR}/env/OMRDebugEnv.cpp
)

if(TR_TARGET_BITS STREQUAL 64)
	compiler_library(x_64
		${CMAKE_CURRENT_LIST_DIR}/amd64/codegen/OMRCodeGenerator.cpp
		${CMAKE_CURRENT_LIST_DIR}/amd64/codegen/OMRMachine.cpp
		${CMAKE_CURRENT_LIST_DIR}/amd64/codegen/OMRTreeEvaluator.cpp
		${CMAKE_CURRENT_LIST_DIR}/amd64/codegen/OMRMemoryReference.cpp
		${CMAKE_CURRENT_LIST_DIR}/amd64/codegen/OMRRealRegister.cpp
		${CMAKE_CURRENT_LIST_DIR}/amd64/codegen/AMD64SystemLinkage.cpp
		${CMAKE_CURRENT_LIST_DIR}/amd64/codegen/OMRELFRelocationResolver.cpp
		${CMAKE_CURRENT_LIST_DIR}/amd64/objectfmt/OMRJitCodeRWXObjectFormat.cpp
		${CMAKE_CURRENT_LIST_DIR}/amd64/objectfmt/OMRJitCodeRXObjectFormat.cpp
	)
else()
	compiler_library(x_32
		${CMAKE_CURRENT_LIST_DIR}/i386/codegen/IA32SystemLinkage.cpp
		${CMAKE_CURRENT_LIST_DIR}/i386/codegen/OMRCodeGenerator.cpp
		${CMAKE_CURRENT_LIST_DIR}/i386/codegen/OMRMachine.cpp
		${CMAKE_CURRENT_LIST_DIR}/i386/codegen/OMRRealRegister.cpp
		${CMAKE_CURRENT_LIST_DIR}/i386/codegen/OMRTreeEvaluator.cpp
	)
endif()
